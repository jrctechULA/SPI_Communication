//____________________________________________________________________________________________________
// Include section:
//____________________________________________________________________________________________________
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "driver/spi_master.h"
#include "SPI_IO_Master.h"

#include "esp_log.h"

//____________________________________________________________________________________________________
// Macro definitions:
//____________________________________________________________________________________________________

#define ledYellow 37
#define ledGreen 36

#define STACK_SIZE 2048

//____________________________________________________________________________________________________
// Global declarations:
//____________________________________________________________________________________________________

DMA_ATTR uint32_t recvbuf[33]={0};
DMA_ATTR uint32_t sendbuf[33]={0};

spi_device_handle_t handle;

typedef struct {
	int** analogTables;   // Vector de apuntadores a los vectores analógicos
    int** digitalTables;  // Vector de apuntadores a los vectores Digitales
	int analogSize;       // Tamaño de los vectores analógicos
    int digitalSize;      // Tamaño de los vectores analógicos
	int numAnTables;      // Número de vectores analógicos
    int numDigTables;     //Número de vectores digitales
} varTables_t;

varTables_t s3Tables;

//____________________________________________________________________________________________________
// Function prototypes:
//____________________________________________________________________________________________________

esp_err_t tablesInit(varTables_t *tables, int numAnTables, int numDigTables, int analogSize, int digitalSize);
esp_err_t tablesPrint(varTables_t *tables);
esp_err_t tablesUnload(varTables_t *tables);
esp_err_t askAnalogTable(varTables_t *Tables, uint32_t tbl);
esp_err_t askDigitalTable(varTables_t *Tables, uint32_t tbl);
esp_err_t askAllTables(varTables_t *Tables);
esp_err_t askAnalogData(varTables_t *Tables, uint32_t tbl, int dataIndex);
esp_err_t askDigitalData(varTables_t *Tables, uint32_t tbl, int dataIndex);
esp_err_t writeAnalogTable(varTables_t *Tables, uint32_t tbl);
esp_err_t writeDigitalTable(varTables_t *Tables, uint32_t tbl);
esp_err_t writeAnalogData(varTables_t *Tables, uint32_t tbl, int dataIndex, uint32_t payload);
esp_err_t writeDigitalData(varTables_t *Tables, uint32_t tbl, int dataIndex, uint32_t payload);
void spi_task(void *pvParameters);

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

    ESP_LOGI(TAG, "Tamaño del objeto: %i bytes\n", sizeof(s3Tables));  //Imprime el tamaño de la estructura, el cual es constante independientemente del número y tamaño de los vectores
    tablesInit(&s3Tables, 3,2,10,3);

    tablesPrint(&s3Tables);

    TaskHandle_t xHandle = NULL;
    xTaskCreate(spi_task,
                "spi_task",
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                &xHandle);

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

esp_err_t tablesInit(varTables_t *tables, int numAnTables, int numDigTables, int analogSize, int digitalSize){
    static const char TAG[] = "tablesInit";

    tables->numAnTables = numAnTables;
    tables->numDigTables = numDigTables;
	tables->analogSize = analogSize;
    tables->digitalSize = digitalSize;

    tables->analogTables =(int **)malloc(tables->numAnTables * sizeof(int*));
    tables->digitalTables =(int **)malloc(tables->numDigTables * sizeof(int*));
    
    if ((tables->analogTables == NULL) || (tables->digitalTables == NULL)){
		ESP_LOGE(TAG, "Error al asignar memoria!\n");
		return -1;
	}

    for (int i=0; i< tables->numAnTables; i++)
	{
		tables->analogTables[i] = (int*)malloc(tables->analogSize * sizeof(int));
		if (tables->analogTables[i] == NULL){
			ESP_LOGE(TAG, "Error al asignar memoria!\n");
			return -1;
		}
        memset(tables->analogTables[i], 0, tables->analogSize * 4);
	}

    for (int i=0; i< tables->numDigTables; i++)
	{
		tables->digitalTables[i] = (int*)malloc(tables->digitalSize * sizeof(int));
		if (tables->digitalTables[i] == NULL){
			ESP_LOGE(TAG, "Error al asignar memoria!\n");
			return -1;
		}
        memset(tables->digitalTables[i], 0, tables->digitalSize * 4);
	}



    ESP_LOGI(TAG, "Las tablas fueron inicializadas en memoria\n");
    return ESP_OK;
}

esp_err_t tablesPrint(varTables_t *tables){
    static const char TAG[] = "tablesPrint";
    //Imprimir los vectores analógicos
	for (int i=0; i<tables->numAnTables; i++){
		ESP_LOGI(TAG, "Vector Analógico: %i", i);
		for (int j=0; j<tables->analogSize; j++){
			printf("%i ",tables->analogTables[i][j]);
		}
		printf("\n");
	}

    //Imprimir los vectores digitales
	for (int i=0; i<tables->numDigTables; i++){
		ESP_LOGI(TAG, "Vector Digital: %i", i);
		for (int j=0; j<tables->digitalSize; j++){
			printf("%i ",tables->digitalTables[i][j]);
		}
		printf("\n");
	}
    return ESP_OK;
}

esp_err_t tablesUnload(varTables_t *tables){
    static const char TAG[] = "tablesUnload";
    for (int i=0; i<tables->numAnTables; i++){  //Libera primero cada vector (int*)
		free(tables->analogTables[i]);
	}
	free(tables->analogTables);                  //Libera el vector de apuntadores (int**)

    for (int i=0; i<tables->numDigTables; i++){  //Libera primero cada vector (int*)
		free(tables->digitalTables[i]);
	}
	free(tables->digitalTables);                  //Libera el vector de apuntadores (int**)
    
    ESP_LOGI(TAG, "Memoria liberada");
    return ESP_OK;
}

esp_err_t askAnalogTable(varTables_t *Tables, uint32_t tbl){
    sendbuf[0] = 1;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_receive(Tables->analogSize);
    //vTaskDelay(pdMS_TO_TICKS(10));
    if (recvbuf[0] != 0xFFFFFFFF){
        for (int j=0; j < Tables->analogSize; j++){
            Tables->analogTables[sendbuf[1]][j] = recvbuf[j];
            recvbuf[j]=0;
            printf("%lu ", (uint32_t)Tables->analogTables[sendbuf[1]][j]);
        }
        printf("\n");
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t askDigitalTable(varTables_t *Tables, uint32_t tbl){
    sendbuf[0] = 2;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_receive(Tables->digitalSize);
    //vTaskDelay(pdMS_TO_TICKS(10));
    if (recvbuf[0] != 0xFFFFFFFF){
        for (int j=0; j < Tables->digitalSize; j++){
            Tables->digitalTables[sendbuf[1]][j] = recvbuf[j];
            recvbuf[j]=0;
            printf("%lu ", (uint32_t)Tables->digitalTables[sendbuf[1]][j]);
        }
        printf("\n");
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t askAllTables(varTables_t *Tables){
    for (int i=0; i < Tables->numAnTables; i++){
        askAnalogTable(Tables, i);
    }
    //vTaskDelay(pdMS_TO_TICKS(5000));



    for (int i=0; i < Tables->numDigTables; i++){
        askDigitalTable(Tables, i);
    }
    //vTaskDelay(pdMS_TO_TICKS(5000));
    return ESP_OK;
}

esp_err_t askAnalogData(varTables_t *Tables, uint32_t tbl, int dataIndex){
    sendbuf[0] = 3;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_receive(1);
    if (recvbuf[0] != 0xFFFFFFFF){
        Tables->analogTables[sendbuf[1]][sendbuf[2]] = recvbuf[0];
        recvbuf[0]=0;
        printf("%lu\n", (uint32_t)Tables->analogTables[sendbuf[1]][sendbuf[2]]);
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;

}

esp_err_t askDigitalData(varTables_t *Tables, uint32_t tbl, int dataIndex){
    sendbuf[0] = 4;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_receive(1);
    if (recvbuf[0] != 0xFFFFFFFF){
        Tables->digitalTables[sendbuf[1]][sendbuf[2]] = recvbuf[0];
        recvbuf[0]=0;
        printf("%lu\n", (uint32_t)Tables->digitalTables[sendbuf[1]][sendbuf[2]]);
    }
    else {
        printf("Communication error! try again...\n");
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t writeAnalogTable(varTables_t *Tables, uint32_t tbl){
    sendbuf[0] = 5;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_write((uint32_t *)s3Tables.analogTables[tbl], s3Tables.analogSize);
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t writeDigitalTable(varTables_t *Tables, uint32_t tbl){
    sendbuf[0] = 6;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
    sendbuf[3] = 0;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_write((uint32_t *)s3Tables.digitalTables[tbl], s3Tables.digitalSize);
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK; 
}

esp_err_t writeAnalogData(varTables_t *Tables, uint32_t tbl, int dataIndex, uint32_t payload){
    sendbuf[0] = 7;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = payload;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(100)); 
    return ESP_OK;
}
esp_err_t writeDigitalData(varTables_t *Tables, uint32_t tbl, int dataIndex, uint32_t payload){
    sendbuf[0] = 8;
    sendbuf[1] = tbl;
    sendbuf[2] = dataIndex;
    sendbuf[3] = payload;
        
    spi_write(sendbuf, 4);
    vTaskDelay(pdMS_TO_TICKS(100)); 
    return ESP_OK;
}

// freeRTOS tasks implementations:
//____________________________________________________________________________________________________
void spi_task(void *pvParameters)
{
    init_spi();
    while (1)
    {
        gpio_set_level(ledYellow,1);

        //______________________________________________________
        //Ask tables testing code:

        //askAllTables(&s3Tables);
        //tablesPrint(&s3Tables);

        //______________________________________________________
        //Ask single data testing code:

        /* for (int i = 0; i < s3Tables.numAnTables; i++){
            for (int j = 0; j < s3Tables.analogSize; j++){
                askAnalogData(&s3Tables, i, j);
            }
        }
        for (int i = 0; i < s3Tables.numDigTables; i++){
            for (int j = 0; j < s3Tables.digitalSize; j++){
                askDigitalData(&s3Tables, i, j);
            }
        } */


        //______________________________________________________
        //Write table testing code:

        for (int i = 0; i < s3Tables.analogSize; i++){
            s3Tables.analogTables[1][i] = i+555;
        }
        writeAnalogTable(&s3Tables, 1);

        for (int i = 0; i < s3Tables.digitalSize; i++){
            s3Tables.digitalTables[1][i] = i+976;
        }
        writeDigitalTable(&s3Tables, 1);
        
        //______________________________________________________
        //Write single data testing code:

        /* for (int i = 0; i < s3Tables.numAnTables; i++){
            for (int j = 0; j < s3Tables.analogSize; j++){
                writeAnalogData(&s3Tables, i, j, i+j);
            }
        }
        for (int i = 0; i < s3Tables.numDigTables; i++){
            for (int j = 0; j < s3Tables.digitalSize; j++){
                writeDigitalData(&s3Tables, i, j, i+j);
            }
        } */
        
        gpio_set_level(ledYellow,0);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}





