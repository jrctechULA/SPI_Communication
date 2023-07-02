#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_slave.h"
#include "esp_log.h"

// Pins in use
#define GPIO_MOSI 12
#define GPIO_MISO 13
#define GPIO_SCLK 15
#define GPIO_CS 14

uint32_t recvbuf[32]={0};
uint32_t sendbuf[32]={0};


static esp_err_t init_spi(void) 
{
    // Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_MOSI,
        .miso_io_num=GPIO_MISO,
        .sclk_io_num=GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    // Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg={
        .mode=0,
        .spics_io_num=GPIO_CS,
        .queue_size=3,
        .flags=0,
    };

    // Initialize SPI slave interface
    spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    return ESP_OK;
}

static esp_err_t spi_receive(int nData)
{
    static const char TAG[] = "SPI Slave";
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32 + 32;
    t.rx_buffer = recvbuf;
    esp_err_t res = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    if (res == ESP_OK)
        ESP_LOGE(TAG, "Received %lu %lu %lu %lu", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3]);
    return ESP_OK;
} 
static esp_err_t spi_write(uint32_t *payload, int nData) 
{
    //static const char TAG[] = "SPI Slave";
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32;
    t.tx_buffer = payload;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    //ESP_LOGI(TAG, "Transmitted %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu ", payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
    return ESP_OK;
}

typedef struct {
	int** analogTables;   // Vector de apuntadores a los vectores analógicos
    int** digitalTables;  // Vector de apuntadores a los vectores Digitales
	int analogSize;       // Tamaño de los vectores analógicos
    int digitalSize;      // Tamaño de los vectores analógicos
	int numAnTables;      // Número de vectores analógicos
    int numDigTables;     //Número de vectores digitales
} varTables_t;

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


void app_main(void)
{
    static const char TAG[] = "Master Main";
    
    varTables_t IOTables;
    ESP_LOGI(TAG, "Tamaño del objeto: %i bytes\n", sizeof(IOTables));  //Imprime el tamaño de la estructura, el cual es constante independientemente del número y tamaño de los vectores
    tablesInit(&IOTables, 3,2,10,3);

    tablesPrint(&IOTables);

    //Rellenar los vectores analógicos:
	ESP_LOGI(TAG, "Rellenando vectores analógicos... %i vectores, Tamaño: %i", IOTables.numAnTables, IOTables.analogSize);

    for (int i=0; i<IOTables.numAnTables; i++){
		for (int j=0; j<IOTables.analogSize; j++){		
			IOTables.analogTables[i][j]= (i+1)*(j+1);
		}
	}

    //Rellenar los vectores digitales:
	ESP_LOGI(TAG, "Rellenando vectores digitales... %i vectores, Tamaño: %i", IOTables.numDigTables, IOTables.digitalSize);
    for (int i=0; i<IOTables.numDigTables; i++){
		for (int j=0; j<IOTables.digitalSize; j++){		
			IOTables.digitalTables[i][j]= (i+1)*(j+1);
		}
	}
    ESP_LOGI(TAG, "Tablas cargadas\n\n");

    tablesPrint(&IOTables);

    //Liberar memoria
	//tablesUnload(&IOTables);

    init_spi();
    int counter = 0;
    while (1)
    {
        // Chequear petición
        recvbuf[0] = 0;
        recvbuf[1] = 0;
        recvbuf[2] = 0;
        spi_receive(3);
        if (recvbuf[0] != 0){  //Valid request
            if(recvbuf[0] == 1){ //Request for analog table
                printf("Requested analog table %li\n", recvbuf[1]);
                printf("%i %i %i %i %i %i %i %i %i %i\n", IOTables.analogTables[recvbuf[1]][0], IOTables.analogTables[recvbuf[1]][1], IOTables.analogTables[recvbuf[1]][2], IOTables.analogTables[recvbuf[1]][3], IOTables.analogTables[recvbuf[1]][4], IOTables.analogTables[recvbuf[1]][5], IOTables.analogTables[recvbuf[1]][6], IOTables.analogTables[recvbuf[1]][7], IOTables.analogTables[recvbuf[1]][8], IOTables.analogTables[recvbuf[1]][9]);
                spi_write(IOTables.analogTables[recvbuf[1]], IOTables.analogSize);
            }
            else if (recvbuf[0] == 2) { //Request for digital table
                printf("Requested digital table %li\n", recvbuf[1]);
                printf("%i %i %i\n", IOTables.digitalTables[recvbuf[1]][0], IOTables.digitalTables[recvbuf[1]][1], IOTables.digitalTables[recvbuf[1]][2]);
                spi_write(IOTables.digitalTables[recvbuf[1]], IOTables.digitalSize);
            }
        }
        else {
            sendbuf[0] = 99;
            spi_write(sendbuf, 1);
            printf("Zero\n");
        }


        //Rellenar los vectores analógicos:
        //ESP_LOGI(TAG, "Rellenando vectores analógicos... %i vectores, Tamaño: %i", IOTables.numAnTables, IOTables.analogSize);

        for (int i=0; i<IOTables.numAnTables; i++){
            for (int j=0; j<IOTables.analogSize; j++){		
                IOTables.analogTables[i][j]= (i+1)*(j+1) + counter;
            }
        }

        //Rellenar los vectores digitales:
        //ESP_LOGI(TAG, "Rellenando vectores digitales... %i vectores, Tamaño: %i", IOTables.numDigTables, IOTables.digitalSize);
        for (int i=0; i<IOTables.numDigTables; i++){
            for (int j=0; j<IOTables.digitalSize; j++){		
                IOTables.digitalTables[i][j]= (i+1)*(j+1) + counter;
            }
        }
        //ESP_LOGI(TAG, "Tablas cargadas\n\n");
        counter++;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
