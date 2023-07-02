#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "esp_log.h"

// Pins in use
#define GPIO_MOSI 4
#define GPIO_MISO 5
#define GPIO_SCLK 6
#define GPIO_CS 7

#define STACK_SIZE 2048

uint32_t recvbuf[33]={0};
uint32_t sendbuf[33]={0};

spi_device_handle_t handle;

static esp_err_t init_spi(void) 
{
    // Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1};

    // Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_8M,   //8MHz
        .duty_cycle_pos = 128, // 50% duty cycle
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .cs_ena_posttrans = 0, // Keep the CS low 0 cycles after transaction
        .queue_size = 3};


    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    return ESP_OK;
}

static esp_err_t spi_write(uint32_t *payload, int nData) 
{
    static const char TAG[] = "SPI Master";
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32;
    t.tx_buffer = payload;
    spi_device_transmit(handle, &t);
    ESP_LOGI(TAG, "Transmitted: %lu %lu %lu", payload[0], payload[1], payload[2]);
    return ESP_OK;
}

static esp_err_t spi_receive(int nData)
{
    //static const char TAG[] = "SPI Master";
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32;
    t.rx_buffer = recvbuf;
    spi_device_transmit(handle, &t);   
    /* ESP_LOGE(TAG, "Received %lu %lu %lu %lu", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3]); */
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

esp_err_t askAnalogTable(varTables_t *Tables, uint32_t tbl){
    sendbuf[0] = 1;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
        
    spi_write(sendbuf, 3);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_receive(Tables->analogSize);
    //vTaskDelay(pdMS_TO_TICKS(10));
    for (int j=0; j < Tables->analogSize; j++){
        Tables->analogTables[sendbuf[1]][j] = recvbuf[j];
        recvbuf[j]=0;
        printf("%lu ", (uint32_t)Tables->analogTables[sendbuf[1]][j]);
    }
    printf("\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t askDigitalTable(varTables_t *Tables, uint32_t tbl){
    sendbuf[0] = 2;
    sendbuf[1] = tbl;
    sendbuf[2] = 0;
        
    spi_write(sendbuf, 3);
    vTaskDelay(pdMS_TO_TICKS(25));
    spi_receive(Tables->digitalSize);
    //vTaskDelay(pdMS_TO_TICKS(10));
    for (int j=0; j < Tables->digitalSize; j++){
        Tables->digitalTables[sendbuf[1]][j] = recvbuf[j];
        recvbuf[j]=0;
        printf("%lu ", (uint32_t)Tables->digitalTables[sendbuf[1]][j]);
    }
    printf("\n");
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

void spi_task(void *pvParameters)
{
    while (1) {
        // Código de la tarea aquí
        printf("\nHola desde la task!\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Retardo de 1 segundo
    }
}



void app_main(void)
{
    static const char TAG[] = "Master Main";

    TaskHandle_t xHandle = NULL;
    xTaskCreate(spi_task,
                "spi_task",
                STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                &xHandle);
    
    varTables_t s3Tables;
    ESP_LOGI(TAG, "Tamaño del objeto: %i bytes\n", sizeof(s3Tables));  //Imprime el tamaño de la estructura, el cual es constante independientemente del número y tamaño de los vectores
    tablesInit(&s3Tables, 3,2,10,3);

    tablesPrint(&s3Tables);

    init_spi();
    
    while (1)
    {
        askAllTables(&s3Tables);
        //tablesPrint(&s3Tables);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    //Liberar memoria
	//tablesUnload(&s3Tables);

}

