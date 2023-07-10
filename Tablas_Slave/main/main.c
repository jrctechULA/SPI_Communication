//____________________________________________________________________________________________________
// Include section:
//____________________________________________________________________________________________________
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"

#include "driver/spi_slave.h"
#include "SPI_IO_Slave.h"

#include "esp_crc.h"

#include "esp_log.h"

#include "esp_timer.h"

//____________________________________________________________________________________________________
// Macro definitions:
//____________________________________________________________________________________________________
#define SPI_BUFFER_SIZE 55

//____________________________________________________________________________________________________
// Global declarations:
//____________________________________________________________________________________________________
//DMA_ATTR uint16_t recvbuf[32]={0};
//DMA_ATTR uint16_t sendbuf[32]={0};
DMA_ATTR WORD_ALIGNED_ATTR uint16_t* recvbuf;
DMA_ATTR WORD_ALIGNED_ATTR uint16_t* sendbuf;

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

uint16_t checksumTable(uint16_t *table, uint8_t nData);

//____________________________________________________________________________________________________
// Main program:
//____________________________________________________________________________________________________
void app_main(void)
{
    static const char TAG[] = "Slave Main";
    
    varTables_t IOTables;
    ESP_LOGI(TAG, "Tamaño del objeto: %i bytes\n", sizeof(IOTables));  //Imprime el tamaño de la estructura, el cual es constante independientemente del número y tamaño de los vectores
    //tablesInit(&IOTables, 3,2,10,3);
    
    tablesInit(&IOTables, 2,    //Tablas de variables analógicas
                          2,    //Tablas de variables digitales
                          1,    //Tablas de configuración
                          1,    //Tablas auxiliares
                          16,   //Tamaño de tablas analógicas
                          2,    //Tamaño de tablas digitales
                          50,   //Tamaño de tablas de configuración
                          50);  //Tamaño de tablas auxiliares

    sendbuf = (uint16_t*)heap_caps_malloc(SPI_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    recvbuf = (uint16_t*)heap_caps_malloc(SPI_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);

    uint16_t payload, crc, crcRecv;
    uint8_t tbl, dataIndex;

    uint64_t tiempoInicio, tiempoFin, tiempoTranscurrido;

    for (int i=0; i<IOTables.configSize; i++)
        IOTables.configTbl[0][i] = 5+i;

    for (int i=0; i<IOTables.auxSize; i++)
        IOTables.auxTbl[0][i] = 100+i;
    
    init_spi();
    while (1)
    {
        // Check request:
        //memset(recvbuf, 0, sizeof(recvbuf));
        memset(recvbuf, 0, 5*2);
        spi_receive(5);
        tbl = recvbuf[1];
        dataIndex = recvbuf[2];
        payload = recvbuf[3];
        
        crcRecv = recvbuf[4];
        crc = checksumTable(recvbuf, 4);
        
        printf("Checksum: Received %u  Calculated %u\n", crcRecv, crc);
        if (crcRecv == crc){
            switch (recvbuf[0])
            {
                case 1:             //Request for analog table
                    //tiempoInicio = esp_timer_get_time();

                    printf("Requested analog table %u\n", tbl);
                    tablePrint(IOTables.anTbl[tbl], IOTables.anSize);
                    //printf("%u %u %u %u %u %u %u %u %u %u\n", IOTables.anTbl[recvbuf[1]][0], IOTables.anTbl[recvbuf[1]][1], IOTables.anTbl[recvbuf[1]][2], IOTables.anTbl[recvbuf[1]][3], IOTables.anTbl[recvbuf[1]][4], IOTables.anTbl[recvbuf[1]][5], IOTables.anTbl[recvbuf[1]][6], IOTables.anTbl[recvbuf[1]][7], IOTables.anTbl[recvbuf[1]][8], IOTables.anTbl[recvbuf[1]][9]);
                    crc = checksumTable(IOTables.anTbl[tbl], IOTables.anSize);
                    for (int i=0; i<IOTables.anSize; i++)
                        sendbuf[i] = IOTables.anTbl[tbl][i];
                    sendbuf[IOTables.anSize] = crc;

                    tiempoInicio = esp_timer_get_time();
                    spi_write(sendbuf, IOTables.anSize + 1);

                    tiempoFin = esp_timer_get_time();
                    tiempoTranscurrido = (tiempoFin - tiempoInicio) / 1000;
                    printf("El tiempo de ejecución fue de %llu milisegundos\n", tiempoTranscurrido);
                    break;

                case 2:             //Request for digital table
                    printf("Requested digital table %u\n", tbl);
                    tablePrint(IOTables.digTbl[tbl], IOTables.digSize);
                    //printf("%u %u %u\n", IOTables.digTbl[recvbuf[1]][0], IOTables.digTbl[recvbuf[1]][1], IOTables.digTbl[recvbuf[1]][2]);
                    crc = checksumTable(IOTables.digTbl[tbl], IOTables.digSize);
                    for (int i=0; i<IOTables.digSize; i++)
                        sendbuf[i] = IOTables.digTbl[tbl][i];
                    sendbuf[IOTables.digSize] = crc;
                    spi_write(sendbuf, IOTables.digSize + 1);
                    break;

                case 3:             //Request for single analog value
                    printf("Requested value %u from analog table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.anTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.anTbl[tbl][dataIndex];
                    crc = checksumTable(sendbuf, 1);
                    sendbuf[1] = crc;
                    spi_write(sendbuf, 2);
                    break;

                case 4:             //Request for single digital value
                    printf("Requested value %u from digital table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.digTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.digTbl[tbl][dataIndex];
                    crc = checksumTable(sendbuf, 1);
                    sendbuf[1] = crc;
                    spi_write(sendbuf, 2);
                    break;

                case 5:             //Write request for analog table
                    printf("Write request to analog table %u\n", tbl);
                    spi_receive(IOTables.anSize + 1);
                    //vTaskDelay(pdMS_TO_TICKS(10));
                    crcRecv = recvbuf[IOTables.anSize];
                    crc = checksumTable(recvbuf, IOTables.anSize);
                    if(crcRecv != crc){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        break;
                    }

                    for (int j=0; j < IOTables.anSize; j++){
                        IOTables.anTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.anTbl[tbl], IOTables.anSize);
                    
                    break;

                case 6:             //Write request for digital table
                    printf("Write request to digital table %u\n", tbl);
                    spi_receive(IOTables.digSize + 1);
                    //vTaskDelay(pdMS_TO_TICKS(10));
                    crcRecv = recvbuf[IOTables.digSize];
                    crc = checksumTable(recvbuf, IOTables.digSize);
                    if(crcRecv != crc){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        break;
                    }
                    
                    for (int j=0; j < IOTables.digSize; j++){
                        IOTables.digTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.digTbl[tbl], IOTables.digSize);
                    break;

                case 7:             //Write request for single analog data
                    IOTables.anTbl[tbl][dataIndex] = payload;
                    printf("Written %u to analog table %u at index %u\n", (uint16_t)IOTables.anTbl[tbl][dataIndex], tbl, dataIndex);
                    break;
                
                case 8:             //Write request for single digital data
                    IOTables.digTbl[tbl][dataIndex] = payload;
                    printf("Written %u to digital table %u at index %u\n", (uint16_t)IOTables.digTbl[tbl][dataIndex], tbl, dataIndex);
                    break;

                case 9:             //Request for config table
                    printf("Requested config table %u\n", tbl);
                    tablePrint(IOTables.configTbl[tbl], IOTables.configSize);
                    crc = checksumTable(IOTables.configTbl[tbl], IOTables.configSize);
                    for (int i=0; i<IOTables.configSize; i++)
                        sendbuf[i] = IOTables.configTbl[tbl][i];
                    sendbuf[IOTables.configSize] = crc;
                    spi_write(sendbuf, IOTables.configSize + 1);
                    break;

                case 10:            //Request for aux table
                    printf("Requested aux table %u\n", tbl);
                    tablePrint(IOTables.auxTbl[tbl], IOTables.auxSize);
                    crc = checksumTable(IOTables.auxTbl[tbl], IOTables.auxSize);
                    for (int i=0; i<IOTables.auxSize; i++)
                        sendbuf[i] = IOTables.auxTbl[tbl][i];
                    sendbuf[IOTables.auxSize] = crc;
                    spi_write(sendbuf, IOTables.auxSize + 1);
                    break;

                case 11:             //Write request for config table
                    printf("Write request to config table %u\n", tbl);
                    spi_receive(IOTables.configSize + 1);

                    crcRecv = recvbuf[IOTables.configSize];
                    crc = checksumTable(recvbuf, IOTables.configSize);
                    if(crcRecv != crc){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        break;
                    }

                    for (int j=0; j < IOTables.configSize; j++){
                        IOTables.configTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.configTbl[tbl], IOTables.configSize);
                    break;

                case 12:             //Write request for aux table
                    printf("Write request to aux table %u\n", tbl);
                    spi_receive(IOTables.auxSize + 1);

                    crcRecv = recvbuf[IOTables.auxSize];
                    crc = checksumTable(recvbuf, IOTables.auxSize);
                    if(crcRecv != crc){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        break;
                    }

                    for (int j=0; j < IOTables.auxSize; j++){
                        IOTables.auxTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.auxTbl[tbl], IOTables.auxSize);
                    break;

                case 13:             //Write request for single config data
                    IOTables.configTbl[tbl][dataIndex] = payload;
                    printf("Written %u to config table %u at index %u\n", (uint16_t)IOTables.configTbl[tbl][dataIndex], tbl, dataIndex);
                    break;

                case 14:             //Write request for single aux data
                    IOTables.auxTbl[tbl][dataIndex] = payload;
                    printf("Written %u to aux table %u at index %u\n", (uint16_t)IOTables.auxTbl[tbl][dataIndex], tbl, dataIndex);
                    break;

                case 15:             //Request for single config value
                    printf("Requested value %u from config table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.configTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.configTbl[tbl][dataIndex];
                    crc = checksumTable(sendbuf, 1);
                    sendbuf[1] = crc;
                    spi_write(sendbuf, 2);
                    break;

                case 16:             //Request for single aux value
                    printf("Requested value %u from aux table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.auxTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.auxTbl[tbl][dataIndex];
                    crc = checksumTable(sendbuf, 1);
                    sendbuf[1] = crc;
                    spi_write(sendbuf, 2);
                    break;

                case 17:
                    tiempoInicio = esp_timer_get_time();

                    sendbuf[0] = 1;
                    spi_write(sendbuf, 1);

                    spi_receive(IOTables.anSize + 1);
                    crcRecv = recvbuf[IOTables.anSize];
                    crc = checksumTable(recvbuf, IOTables.anSize);
                    if (crcRecv != crc){
                        ESP_LOGE(TAG, "Communication CRC16 error - Bad checksum!");
                        break;
                    }
                    for (int i=0; i<IOTables.anSize; i++)
                        IOTables.anTbl[0][i] = recvbuf[i];

                    spi_receive(IOTables.anSize + 1);
                    crcRecv = recvbuf[IOTables.anSize];
                    crc = checksumTable(recvbuf, IOTables.anSize);
                    if (crcRecv != crc){
                        ESP_LOGE(TAG, "Communication CRC16 error - Bad checksum!");
                        break;
                    }
                    for (int i=0; i<IOTables.anSize; i++)
                        IOTables.anTbl[1][i] = recvbuf[i];

                    spi_receive(IOTables.digSize + 1);
                    crcRecv = recvbuf[IOTables.digSize];
                    crc = checksumTable(recvbuf, IOTables.digSize);
                    if (crcRecv != crc){
                        ESP_LOGE(TAG, "Communication CRC16 error - Bad checksum!");
                        break;
                    }
                    for (int i=0; i<IOTables.digSize; i++)
                        IOTables.digTbl[0][i] = recvbuf[i];

                    spi_receive(IOTables.digSize + 1);
                    crcRecv = recvbuf[IOTables.digSize];
                    crc = checksumTable(recvbuf, IOTables.digSize);
                    if (crcRecv != crc){
                        ESP_LOGE(TAG, "Communication CRC16 error - Bad checksum!");
                        break;
                    }
                    for (int i=0; i<IOTables.digSize; i++)
                        IOTables.digTbl[1][i] = recvbuf[i];

                    tiempoFin = esp_timer_get_time();
                    tiempoTranscurrido = (tiempoFin - tiempoInicio) / 1000;
                    printf("El tiempo de ejecución fue de %llu milisegundos\n", tiempoTranscurrido);

                    tablePrint( IOTables.anTbl[0],  IOTables.anSize);
                    tablePrint( IOTables.anTbl[1],  IOTables.anSize);
                    tablePrint( IOTables.digTbl[0],  IOTables.digSize);
                    tablePrint( IOTables.digTbl[1],  IOTables.digSize);
                    break;

                default:            //Command not recognized
                    sendbuf[0] = 0xFFFF;
                    spi_write(sendbuf, 1);
                    printf("Command not recognized! %u\n", sendbuf[0]);
            }
        }
        else{                       //Communication CRC16 error - Bad checksum
            sendbuf[0] = 0xFFFF;
            spi_write(sendbuf, 1);
            ESP_LOGE(TAG, "Communication CRC16 error - Bad checksum!");
        }
        
        //Actualizar los vectores analógicos:
        //ESP_LOGI(TAG, "Actualizando vectores analógicos... %u vectores, Tamaño: %u", IOTables.numAnTbls, IOTables.anSize);
        for (int i=0; i<IOTables.numAnTbls; i++){
            for (int j=0; j<IOTables.anSize; j++){		
                IOTables.anTbl[i][j] += 1;
            }
        }

        //Actualizar los vectores digitales:
        //ESP_LOGI(TAG, "Actualizando vectores digitales... %u vectores, Tamaño: %u", IOTables.numDigTbls, IOTables.digSize);
        for (int i=0; i<IOTables.numDigTbls; i++){
            for (int j=0; j<IOTables.digSize; j++){		
                IOTables.digTbl[i][j] += 1;
            }
        }
        //ESP_LOGI(TAG, "Tablas cargadas\n\n");

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    //Free memory
	//tablesUnload(&IOTables);
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

uint16_t checksumTable(uint16_t *table, uint8_t nData){
    //size_t data_len = sizeof(table) / 2;
    uint16_t crc = 0xFFFF; // Valor inicial del CRC-16
    
    for (size_t i = 0; i < nData; i++) {
        uint8_t* bytes = (uint8_t*)&table[i];
        size_t num_bytes = sizeof(table[i]);
        
        for (size_t j = 0; j < num_bytes; j++) {
            //crc = esp_crc16_le(&bytes[j], 1, crc);
            crc = esp_crc16_le(crc, &bytes[j], 1);
        }
    }
    return crc;
}