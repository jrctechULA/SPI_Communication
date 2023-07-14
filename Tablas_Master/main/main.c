//____________________________________________________________________________________________________
// Include section:
//____________________________________________________________________________________________________
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"

#include "driver/spi_master.h"
#include "SPI_IO_Master.h"

#include "esp_log.h"

#include "esp_timer.h"

//____________________________________________________________________________________________________
// Macro definitions:
//____________________________________________________________________________________________________

#define ledYellow 37
#define ledGreen 36

#define STACK_SIZE 3072
#define SPI_BUFFER_SIZE 55

//____________________________________________________________________________________________________
// Global declarations:
//____________________________________________________________________________________________________

DMA_ATTR WORD_ALIGNED_ATTR uint16_t* recvbuf;
DMA_ATTR WORD_ALIGNED_ATTR uint16_t* sendbuf;

spi_device_handle_t handle;

typedef struct {
	uint16_t** anTbl;      // Vector de apuntadores a los vectores analógicos
    uint16_t** digTbl;     // Vector de apuntadores a los vectores Digitales
    uint16_t** configTbl;  // Vector de apuntadores a los vectores de configuración
    uint16_t** auxTbl;     // Vector de apuntadores a los vectores auxiliares
	uint8_t anSize;        // Tamaño de los vectores analógicos
    uint8_t digSize;       // Tamaño de los vectores analógicos
    uint8_t configSize;    // Tamaño de los vectores de configuración
    uint8_t auxSize;       // Tamaño de los vectores auxiliares
	uint8_t numAnTbls;     // Número de vectores analógicos
    uint8_t numDigTbls;    // Número de vectores digitales
    uint8_t numConfigTbls; // Número de vectores de configuración
    uint8_t numAuxTbls;    // Número de vectores auxiliares
} varTables_t;

varTables_t s3Tables;

//The semaphore indicating the slave is ready to receive stuff.
QueueHandle_t rdySem;

//____________________________________________________________________________________________________
// Function prototypes:
//____________________________________________________________________________________________________

//esp_err_t tablesInit(varTables_t *tables, uint8_t numAnTbls, uint8_t numDigTbls, uint8_t anSize, uint8_t digSize);
esp_err_t tablesInit(varTables_t *tables, 
                     uint8_t numAnTbls,     //Tablas de variables analógicas
                     uint8_t numDigTbls,    //Tablas de variables digitales
                     uint8_t numConfigTbls, //Tablas de configuración
                     uint8_t numAuxTbls,    //Tablas auxiliares
                     uint8_t anSize,        //Tamaño de tablas analógicas
                     uint8_t digSize,       //Tamaño de tablas digitales
                     uint8_t configSize,    //Tamaño de tablas de configuración
                     uint8_t auxSize);      //Tamaño de tablas auxiliares

esp_err_t tablePrint(uint16_t *table, uint8_t size);
esp_err_t tablesUnload(varTables_t *tables);
esp_err_t readAnalogTable(varTables_t *Tables, uint8_t tbl);
esp_err_t readDigitalTable(varTables_t *Tables, uint8_t tbl);
esp_err_t readConfigTable(varTables_t *Tables, uint8_t tbl);
esp_err_t readAuxTable(varTables_t *Tables, uint8_t tbl);
esp_err_t readAllTables(varTables_t *Tables);
esp_err_t readAnalogData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex);
esp_err_t readDigitalData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex);
esp_err_t readConfigData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex);
esp_err_t readAuxData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex);
esp_err_t writeAnalogTable(varTables_t *Tables, uint8_t tbl);
esp_err_t writeDigitalTable(varTables_t *Tables, uint8_t tbl);
esp_err_t writeConfigTable(varTables_t *Tables, uint8_t tbl);
esp_err_t writeAuxTable(varTables_t *Tables, uint8_t tbl);
esp_err_t writeAnalogData(uint8_t tbl, uint8_t dataIndex, uint16_t payload);
esp_err_t writeDigitalData(uint8_t tbl, uint8_t dataIndex, uint16_t payload);
esp_err_t writeConfigData(uint8_t tbl, uint8_t dataIndex, uint16_t payload);
esp_err_t writeAuxData(uint8_t tbl, uint8_t dataIndex, uint16_t payload);

esp_err_t exchangeData(varTables_t *Tables);

//uint16_t checksumTable(uint16_t *table, uint8_t nData);

void spi_task(void *pvParameters);


/*
This ISR is called when the handshake line goes high.
*/
static void IRAM_ATTR gpio_handshake_isr_handler(void* arg)
{
    //Sometimes due to interference or ringing or something, we get two irqs after eachother. This is solved by
    //looking at the time between interrupts and refusing any interrupt too close to another one.
    static uint32_t lasthandshaketime_us;
    uint32_t currtime_us = esp_timer_get_time();
    uint32_t diff = currtime_us - lasthandshaketime_us;
    if (diff < 100) {
        return; //ignore everything <1ms after an earlier irq
    }
    lasthandshaketime_us = currtime_us;

    //Give the semaphore.
    BaseType_t mustYield = false;
    xSemaphoreGiveFromISR(rdySem, &mustYield);
    if (mustYield) {
        portYIELD_FROM_ISR();
    }
}

//____________________________________________________________________________________________________
// Main program:
//____________________________________________________________________________________________________
void app_main(void)
{
    static const char TAG[] = "Master Main";

    gpio_reset_pin(ledYellow);
    gpio_set_direction(ledYellow, GPIO_MODE_OUTPUT);
    gpio_set_level(ledYellow,0);

    gpio_reset_pin(ledGreen);
    gpio_set_direction(ledGreen, GPIO_MODE_OUTPUT);
    gpio_set_level(ledGreen,0);

    //Set up handshake line interrupt.
    //GPIO config for the handshake line.
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_POSEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=1,
        .pin_bit_mask=(1<<GPIO_HANDSHAKE)
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_set_intr_type(GPIO_HANDSHAKE, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(GPIO_HANDSHAKE, gpio_handshake_isr_handler, NULL);

    ESP_LOGI(TAG, "Tamaño del objeto: %i bytes\n", sizeof(s3Tables));  //Imprime el tamaño de la estructura, el cual es constante independientemente del número y tamaño de los vectores
    //tablesInit(&s3Tables, 3,2,10,3);
    tablesInit(&s3Tables, 2,    //Tablas de variables analógicas
                          2,    //Tablas de variables digitales
                          1,    //Tablas de configuración
                          1,    //Tablas auxiliares
                          16,   //Tamaño de tablas analógicas
                          2,    //Tamaño de tablas digitales
                          50,   //Tamaño de tablas de configuración
                          50);  //Tamaño de tablas auxiliares

    sendbuf = (uint16_t*)heap_caps_malloc(SPI_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    recvbuf = (uint16_t*)heap_caps_malloc(SPI_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);

    TaskHandle_t xHandle = NULL;
    xTaskCreate(spi_task,
                "spi_task",
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                &xHandle);

    //Create the semaphore.
    rdySem=xSemaphoreCreateBinary();



    while (1){
        gpio_set_level(ledGreen,0);    
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(ledGreen,1);    
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    //Liberar memoria
	//tablesUnload(&s3Tables);

}

//____________________________________________________________________________________________________
// Function implementations:
//____________________________________________________________________________________________________

// Tables and dynamic memory related functions:
//____________________________________________________________________________________________________

esp_err_t tablesInit(varTables_t *tables, 
                     uint8_t numAnTbls,
                     uint8_t numDigTbls,
                     uint8_t numConfigTbls,
                     uint8_t numAuxTbls,
                     uint8_t anSize,
                     uint8_t digSize,
                     uint8_t configSize,
                     uint8_t auxSize)
{
    static const char TAG[] = "tablesInit";

    tables->numAnTbls = numAnTbls;
    tables->numDigTbls = numDigTbls;
    tables->numConfigTbls = numConfigTbls;
    tables->numAuxTbls = numAuxTbls;
	tables->anSize = anSize;
    tables->digSize = digSize;
    tables->configSize = configSize;
    tables->auxSize = auxSize;

    tables->anTbl =(uint16_t **)malloc(tables->numAnTbls * sizeof(uint16_t*));
    if (tables->anTbl == NULL){
        ESP_LOGE(TAG, "Error al asignar memoria!\n");
		return ESP_FAIL;
    }

    tables->digTbl =(uint16_t **)malloc(tables->numDigTbls * sizeof(uint16_t*));
    if (tables->digTbl == NULL){
		ESP_LOGE(TAG, "Error al asignar memoria!\n");
		return ESP_FAIL;
	}
    
    tables->configTbl =(uint16_t **)malloc(tables->numConfigTbls * sizeof(uint16_t*));
    if (tables->configTbl == NULL){
		ESP_LOGE(TAG, "Error al asignar memoria!\n");
		return ESP_FAIL;
	}

    tables->auxTbl =(uint16_t **)malloc(tables->numAuxTbls * sizeof(uint16_t*));
    if (tables->auxTbl == NULL){
		ESP_LOGE(TAG, "Error al asignar memoria!\n");
		return ESP_FAIL;
	}

    for (int i=0; i< tables->numAnTbls; i++)
	{
		tables->anTbl[i] = (uint16_t*)malloc(tables->anSize * sizeof(uint16_t));
		if (tables->anTbl[i] == NULL){
			ESP_LOGE(TAG, "Error al asignar memoria!\n");
			return ESP_FAIL;
		}
        memset(tables->anTbl[i], 0, tables->anSize * 2);
	}

    for (int i=0; i< tables->numDigTbls; i++)
	{
		tables->digTbl[i] = (uint16_t*)malloc(tables->digSize * sizeof(uint16_t));
		if (tables->digTbl[i] == NULL){
			ESP_LOGE(TAG, "Error al asignar memoria!\n");
			return ESP_FAIL;
		}
        memset(tables->digTbl[i], 0, tables->digSize * 2);
	}

    for (int i=0; i< tables->numConfigTbls; i++)
	{
		tables->configTbl[i] = (uint16_t*)malloc(tables->configSize * sizeof(uint16_t));
		if (tables->configTbl[i] == NULL){
			ESP_LOGE(TAG, "Error al asignar memoria!\n");
			return ESP_FAIL;
		}
        memset(tables->configTbl[i], 0, tables->configSize * 2);
	}

    for (int i=0; i< tables->numAuxTbls; i++)
	{
		tables->auxTbl[i] = (uint16_t*)malloc(tables->auxSize * sizeof(uint16_t));
		if (tables->auxTbl[i] == NULL){
			ESP_LOGE(TAG, "Error al asignar memoria!\n");
			return ESP_FAIL;
		}
        memset(tables->auxTbl[i], 0, tables->auxSize * 2);
	}


    ESP_LOGI(TAG, "Las tablas fueron inicializadas en memoria\n");
    return ESP_OK;
}

esp_err_t tablePrint(uint16_t *table, uint8_t size){
	for (int i=0; i < size; i++){
	    printf("%u ",table[i]);
	}
    printf("\n");
    return ESP_OK;
}

esp_err_t tablesUnload(varTables_t *tables){
    static const char TAG[] = "tablesUnload";
    for (int i=0; i<tables->numAnTbls; i++){  //Libera primero cada vector (int*)
		free(tables->anTbl[i]);
	}
	free(tables->anTbl);                  //Libera el vector de apuntadores (int**)

    for (int i=0; i<tables->numDigTbls; i++){  //Libera primero cada vector (int*)
		free(tables->digTbl[i]);
	}
	free(tables->digTbl);                  //Libera el vector de apuntadores (int**)
    
    for (int i=0; i<tables->numConfigTbls; i++){  //Libera primero cada vector (int*)
		free(tables->configTbl[i]);
	}
	free(tables->configTbl);                  //Libera el vector de apuntadores (int**)

    for (int i=0; i<tables->numAuxTbls; i++){  //Libera primero cada vector (int*)
		free(tables->auxTbl[i]);
	}
	free(tables->auxTbl);                  //Libera el vector de apuntadores (int**)

    ESP_LOGI(TAG, "Memoria liberada");
    return ESP_OK;
}

esp_err_t readAnalogTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 1;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
            
    spi_write(sendbuf, 4);
    
    spi_receive(Tables->anSize);

    if (recvbuf[0] != 0xFFFF){
        for (int j=0; j < Tables->anSize; j++){
            Tables->anTbl[tbl][j] = recvbuf[j];
            recvbuf[j]=0;
        }
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t readDigitalTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 2;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);   
    
    spi_receive(Tables->digSize);

    if (recvbuf[0] != 0xFFFF){
        for (int j=0; j < Tables->digSize; j++){
            Tables->digTbl[tbl][j] = recvbuf[j];
            recvbuf[j]=0;
        }
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t readConfigTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 9;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);  

    spi_receive(Tables->configSize);

    if (recvbuf[0] != 0xFFFF){
        for (int j=0; j < Tables->configSize; j++){
            Tables->configTbl[tbl][j] = recvbuf[j];
            recvbuf[j]=0;
        }
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t readAuxTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 10;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);

    spi_receive(Tables->auxSize);

    if (recvbuf[0] != 0xFFFF){
        for (int j=0; j < Tables->auxSize; j++){
            Tables->auxTbl[tbl][j] = recvbuf[j];
            recvbuf[j]=0;
        }
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t readAllTables(varTables_t *Tables){
    for (int i=0; i < Tables->numAnTbls; i++){
        esp_err_t r = readAnalogTable(Tables, i);
        if (r == ESP_OK)
            tablePrint(Tables->anTbl[i], Tables->anSize);
    }

    for (int i=0; i < Tables->numDigTbls; i++){
        esp_err_t r = readDigitalTable(Tables, i);
        if (r == ESP_OK)
            tablePrint(Tables->digTbl[i], Tables->digSize);
    }
    return ESP_OK;
}

esp_err_t readAnalogData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex){
    sendbuf[0] = 3;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);

    spi_receive(1);
    
    if (recvbuf[0] != 0xFFFF){
        Tables->anTbl[tbl][dataIndex] = recvbuf[0];
        recvbuf[0]=0;
        printf("%u\n", (uint16_t)Tables->anTbl[tbl][dataIndex]);
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;

}

esp_err_t readDigitalData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex){
    sendbuf[0] = 4;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);

    spi_receive(1);

    if (recvbuf[0] != 0xFFFF){
        Tables->digTbl[tbl][dataIndex] = recvbuf[0];
        recvbuf[0]=0;
        printf("%u\n", (uint16_t)Tables->digTbl[tbl][dataIndex]);
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t readConfigData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex){
    sendbuf[0] = 15;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    
    spi_receive(1);

    if (recvbuf[0] != 0xFFFF){
        Tables->configTbl[tbl][dataIndex] = recvbuf[0];
        recvbuf[0]=0;
        printf("%u\n", (uint16_t)Tables->configTbl[tbl][dataIndex]);
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t readAuxData(varTables_t *Tables, uint8_t tbl, uint8_t dataIndex){
    sendbuf[0] = 16;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    
    spi_receive(1);

    if (recvbuf[0] != 0xFFFF){
        Tables->auxTbl[tbl][dataIndex] = recvbuf[0];
        recvbuf[0]=0;
        printf("%u\n", (uint16_t)Tables->auxTbl[tbl][dataIndex]);
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t writeAnalogTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 5;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);

    for (int i=0; i<s3Tables.anSize; i++)
        sendbuf[i] = s3Tables.anTbl[tbl][i];
    spi_write(sendbuf, s3Tables.anSize);
    return ESP_OK;
}

esp_err_t writeDigitalTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 6;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);

    for (int i=0; i<s3Tables.digSize; i++)
        sendbuf[i] = s3Tables.digTbl[tbl][i];
    spi_write(sendbuf, s3Tables.digSize);
    return ESP_OK; 
}

esp_err_t writeConfigTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 11;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);

    for (int i=0; i<s3Tables.configSize; i++)
        sendbuf[i] = s3Tables.configTbl[tbl][i];
    spi_write(sendbuf, s3Tables.configSize);
    return ESP_OK;
}

esp_err_t writeAuxTable(varTables_t *Tables, uint8_t tbl){
    sendbuf[0] = 12;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);

    for (int i=0; i<s3Tables.auxSize; i++)
        sendbuf[i] = s3Tables.auxTbl[tbl][i];
    spi_write(sendbuf, s3Tables.auxSize);
    return ESP_OK;
}

esp_err_t writeAnalogData(uint8_t tbl, uint8_t dataIndex, uint16_t payload){
    sendbuf[0] = 7;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = payload;
       
    spi_write(sendbuf, 4);
    return ESP_OK;
}

esp_err_t writeDigitalData(uint8_t tbl, uint8_t dataIndex, uint16_t payload){
    sendbuf[0] = 8;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = payload;
        
    spi_write(sendbuf, 4);
    return ESP_OK;
}

esp_err_t writeConfigData(uint8_t tbl, uint8_t dataIndex, uint16_t payload){
    sendbuf[0] = 13;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = payload;
        
    spi_write(sendbuf, 4);
    return ESP_OK;
}

esp_err_t writeAuxData(uint8_t tbl, uint8_t dataIndex, uint16_t payload){
    sendbuf[0] = 14;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = payload;
        
    spi_write(sendbuf, 4);
    return ESP_OK;
}

esp_err_t exchangeData(varTables_t *Tables){
    //uint64_t tiempoInicio = esp_timer_get_time();

    sendbuf[0] = 17;
    sendbuf[1] = 0;
    sendbuf[2] = 0;
    sendbuf[3] = 0;

    //sendbuf[4] = checksumTable(sendbuf, 4);

    spi_write(sendbuf, 4);

    for (int i=0; i<Tables->anSize; i++){
        sendbuf[i] = Tables->anTbl[1][i];
    }
    for (int i=0; i<Tables->digSize; i++){
        sendbuf[Tables->anSize + i] = Tables->digTbl[1][i];
    }

    uint64_t tiempoInicio = esp_timer_get_time();

    spi_exchange(Tables->anSize + Tables->digSize);

    uint64_t tiempoFin = esp_timer_get_time();

    //Recover data from recvbuf here:
    for (int i=0; i<Tables->anSize; i++)
        Tables->anTbl[0][i] = recvbuf[i];
    for (int i=0; i<Tables->digSize; i++)
        Tables->digTbl[0][i] = recvbuf[Tables->anSize + i];

    //uint64_t tiempoFin = esp_timer_get_time();
    uint64_t tiempoTranscurrido = (tiempoFin - tiempoInicio) / 1000;
    printf("El tiempo de ejecución fue de %llu milisegundos\n", tiempoTranscurrido);
    tablePrint(Tables->anTbl[0],  Tables->anSize);
    tablePrint(Tables->anTbl[1],  Tables->anSize);
    tablePrint(Tables->digTbl[0], Tables->digSize);
    tablePrint(Tables->digTbl[1], Tables->digSize);
    //vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

// freeRTOS tasks implementations:
//____________________________________________________________________________________________________
void spi_task(void *pvParameters)
{
    init_spi();

    //xSemaphoreGive(rdySem);

    //______________________________________________________
    //Dummy transaction
    //______________________________________________________
    /* do {
        sendbuf[0] = 55;
        spi_write(sendbuf, 4);
        spi_receive(1);
        printf("%u\n", recvbuf[0]);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    while (recvbuf[0] != 1); */
    

    //______________________________________________________
    //Some test code to run once...  (See serial terminal for debug)
    //______________________________________________________

    //Write config table 0:
    for (int i=0; i<s3Tables.configSize; i++)
        s3Tables.configTbl[0][i] = 1;
    writeConfigTable(&s3Tables, 0);

    //Write aux table 0:
    for (int i=0; i<s3Tables.auxSize; i++)
        s3Tables.auxTbl[0][i] = 2;
    writeAuxTable(&s3Tables, 0);

    //Write config and aux data at index 15:
    writeConfigData(0, 15, 99);
    writeAuxData(0, 15, 88);

    //Read config and aux data at index 15:
    readConfigData(&s3Tables, 0, 15);
    readAuxData(&s3Tables, 0, 15);

    while (1)
    {
        //Some test code to run repeteadly...

        gpio_set_level(ledYellow,1);

        //______________________________________________________
        //Ask tables testing code:

        //readAllTables(&s3Tables);
        

        //______________________________________________________
        //Ask single data testing code:

        /* for (int i = 0; i < s3Tables.numAnTbls; i++){
            for (int j = 0; j < s3Tables.anSize; j++){
                readAnalogData(&s3Tables, i, j);
            }
        }
        for (int i = 0; i < s3Tables.numDigTbls; i++){
            for (int j = 0; j < s3Tables.digSize; j++){
                readDigitalData(&s3Tables, i, j);
            }
        } */


        //______________________________________________________
        //Write table testing code:

        /* for (int i = 0; i < s3Tables.anSize; i++){
            s3Tables.anTbl[1][i] = i+555;
        }
        writeAnalogTable(&s3Tables, 1);

        for (int i = 0; i < s3Tables.digSize; i++){
            s3Tables.digTbl[1][i] = i+123;
        }
        writeDigitalTable(&s3Tables, 1); */
        
        //______________________________________________________
        //Write single data testing code:

        /* for (int i = 0; i < s3Tables.numAnTbls; i++){
            for (int j = 0; j < s3Tables.anSize; j++){
                writeAnalogData(i, j, 5*j);
            }
        }
        for (int i = 0; i < s3Tables.numDigTbls; i++){
            for (int j = 0; j < s3Tables.digSize; j++){
                writeDigitalData(i, j, 5*j);
            }
        } */
        

        //______________________________________________________
        //Ask config and aux tables testing code:

        /* readConfigTable(&s3Tables, 0);
        tablePrint(s3Tables.configTbl[0], s3Tables.configSize);

        readAuxTable(&s3Tables, 0);
        tablePrint(s3Tables.auxTbl[0], s3Tables.auxSize); */

        //______________________________________________________
        /* readAnalogData(&s3Tables, 0, 1);
        readDigitalData(&s3Tables, 0, 1);
        readConfigData(&s3Tables, 0, 15);
        readAuxData(&s3Tables, 0, 15); */

        
        //______________________________________________________
        for (int i=0; i<s3Tables.anSize; i++){
            s3Tables.anTbl[1][i] +=5;
        }

        for (int i=0; i<s3Tables.digSize; i++){
            s3Tables.digTbl[1][i] +=5;
        }
        exchangeData(&s3Tables);

        gpio_set_level(ledYellow,0);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}





