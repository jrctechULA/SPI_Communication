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

#include "esp_log.h"

#include "esp_timer.h"

//____________________________________________________________________________________________________
// Macro definitions:
//____________________________________________________________________________________________________
#define SPI_BUFFER_SIZE 55
#define SPI_ERROR_COUNT IOTables.auxTbl[1][1]

//____________________________________________________________________________________________________
// Global declarations:
//____________________________________________________________________________________________________

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



//____________________________________________________________________________________________________
// Main program:
//____________________________________________________________________________________________________
void app_main(void)
{
    gpio_reset_pin(GPIO_HANDSHAKE);
    gpio_set_direction(GPIO_HANDSHAKE, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_HANDSHAKE,0);

    static const char TAG[] = "Slave Main";
    
    varTables_t IOTables;
    ESP_LOGI(TAG, "Tamaño del objeto: %i bytes\n", sizeof(IOTables));  //Imprime el tamaño de la estructura, el cual es constante independientemente del número y tamaño de los vectores
    
    tablesInit(&IOTables, 2,    //Tablas de variables analógicas
                          2,    //Tablas de variables digitales
                          1,    //Tablas de configuración
                          2,    //Tablas auxiliares             //Segunda tabla auxiliar para registros propios de la Placa IO
                          16,   //Tamaño de tablas analógicas
                          2,    //Tamaño de tablas digitales
                          50,   //Tamaño de tablas de configuración
                          50);  //Tamaño de tablas auxiliares

    sendbuf = (uint16_t*)heap_caps_malloc(SPI_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    recvbuf = (uint16_t*)heap_caps_malloc(SPI_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);

    uint16_t payload;
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

        memset(recvbuf, 0, 4*2);

        esp_err_t r = spi_receive(4);

        if (r == ESP_OK){
            SPI_ERROR_COUNT = 0;
            tbl = recvbuf[1];
            dataIndex = recvbuf[2];
            payload = recvbuf[3];

            switch (recvbuf[0])
            {             
                case 1:             //Request for analog table
                    printf("Requested analog table %u\n", tbl);
                    tablePrint(IOTables.anTbl[tbl], IOTables.anSize);
                    
                    for (int i=0; i<IOTables.anSize; i++)
                        sendbuf[i] = IOTables.anTbl[tbl][i];

                    tiempoInicio = esp_timer_get_time();

                    spi_write(sendbuf, IOTables.anSize);

                    tiempoFin = esp_timer_get_time();
                    tiempoTranscurrido = (tiempoFin - tiempoInicio) / 1000;
                    printf("El tiempo de ejecución fue de %llu milisegundos\n", tiempoTranscurrido);
                    break;

                case 2:             //Request for digital table
                    printf("Requested digital table %u\n", tbl);
                    tablePrint(IOTables.digTbl[tbl], IOTables.digSize);
                    
                    for (int i=0; i<IOTables.digSize; i++)
                        sendbuf[i] = IOTables.digTbl[tbl][i];
                    
                    spi_write(sendbuf, IOTables.digSize);
                    break;

                case 3:             //Request for single analog value
                    printf("Requested value %u from analog table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.anTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.anTbl[tbl][dataIndex];
                    
                    spi_write(sendbuf, 1);
                    break;

                case 4:             //Request for single digital value
                    printf("Requested value %u from digital table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.digTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.digTbl[tbl][dataIndex];
                    
                    spi_write(sendbuf, 1);
                    break;

                case 5:             //Write request for analog table
                    printf("Write request to analog table %u\n", tbl);
                    r = spi_receive(IOTables.anSize);
                    
                    if(r != ESP_OK){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        sendbuf[0] = 0xFFFF;            //Report communication error to the master
                        spi_write(sendbuf, 1);
                        break;
                    }

                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    for (int j=0; j < IOTables.anSize; j++){
                        IOTables.anTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.anTbl[tbl], IOTables.anSize);                   
                    break;

                case 6:             //Write request for digital table
                    printf("Write request to digital table %u\n", tbl);
                    r = spi_receive(IOTables.digSize);
                    
                    if(r != ESP_OK){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        sendbuf[0] = 0xFFFF;            //Report communication error to the master
                        spi_write(sendbuf, 1);
                        break;
                    }
                    
                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    for (int j=0; j < IOTables.digSize; j++){
                        IOTables.digTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.digTbl[tbl], IOTables.digSize);
                    break;

                case 7:             //Write request for single analog data
                    IOTables.anTbl[tbl][dataIndex] = payload;

                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    printf("Written %u to analog table %u at index %u\n", (uint16_t)IOTables.anTbl[tbl][dataIndex], tbl, dataIndex);
                    break;
                
                case 8:             //Write request for single digital data
                    IOTables.digTbl[tbl][dataIndex] = payload;
                    
                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    printf("Written %u to digital table %u at index %u\n", (uint16_t)IOTables.digTbl[tbl][dataIndex], tbl, dataIndex);
                    break;

                case 9:             //Request for config table
                    printf("Requested config table %u\n", tbl);
                    tablePrint(IOTables.configTbl[tbl], IOTables.configSize);
                    
                    for (int i=0; i<IOTables.configSize; i++)
                        sendbuf[i] = IOTables.configTbl[tbl][i];
                    
                    spi_write(sendbuf, IOTables.configSize);
                    break;

                case 10:            //Request for aux table
                    printf("Requested aux table %u\n", tbl);
                    tablePrint(IOTables.auxTbl[tbl], IOTables.auxSize);
                    
                    for (int i=0; i<IOTables.auxSize; i++)
                        sendbuf[i] = IOTables.auxTbl[tbl][i];
                    
                    spi_write(sendbuf, IOTables.auxSize);
                    break;

                case 11:             //Write request for config table
                    printf("Write request to config table %u\n", tbl);
                    r = spi_receive(IOTables.configSize);

                    if(r != ESP_OK){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        sendbuf[0] = 0xFFFF;            //Report communication error to the master
                        spi_write(sendbuf, 1);
                        break;
                    }

                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    for (int j=0; j < IOTables.configSize; j++){
                        IOTables.configTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.configTbl[tbl], IOTables.configSize);
                    break;

                case 12:             //Write request for aux table
                    printf("Write request to aux table %u\n", tbl);
                    r = spi_receive(IOTables.auxSize);

                    if(r != ESP_OK){
                        ESP_LOGE(TAG, "CRC Error - Bad Checksum!");
                        sendbuf[0] = 0xFFFF;            //Report communication error to the master
                        spi_write(sendbuf, 1);
                        break;
                    }

                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    for (int j=0; j < IOTables.auxSize; j++){
                        IOTables.auxTbl[tbl][j] = recvbuf[j];
                        recvbuf[j]=0;
                    }
                    tablePrint(IOTables.auxTbl[tbl], IOTables.auxSize);
                    break;

                case 13:             //Write request for single config data
                    IOTables.configTbl[tbl][dataIndex] = payload;
                    
                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    printf("Written %u to config table %u at index %u\n", (uint16_t)IOTables.configTbl[tbl][dataIndex], tbl, dataIndex);
                    break;

                case 14:             //Write request for single aux data
                    IOTables.auxTbl[tbl][dataIndex] = payload;
                    
                    sendbuf[0] = 0x0;                   //Report success to the master
                    spi_write(sendbuf, 1);

                    printf("Written %u to aux table %u at index %u\n", (uint16_t)IOTables.auxTbl[tbl][dataIndex], tbl, dataIndex);
                    break;

                case 15:             //Request for single config value
                    printf("Requested value %u from config table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.configTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.configTbl[tbl][dataIndex];
                    
                    spi_write(sendbuf, 1);
                    break;

                case 16:             //Request for single aux value
                    printf("Requested value %u from aux table %u\n", dataIndex, tbl);
                    printf("%u\n", IOTables.auxTbl[tbl][dataIndex]);
                    sendbuf[0] = IOTables.auxTbl[tbl][dataIndex];
                    
                    spi_write(sendbuf, 1);
                    break;

                case 17:             //Request to exchange input and output vars         
                    tiempoInicio = esp_timer_get_time();

                    for (int i=0; i<IOTables.anSize; i++){
                        sendbuf[i] = IOTables.anTbl[0][i];
                    }
                    for (int i=0; i<IOTables.digSize; i++){
                        sendbuf[IOTables.anSize + i] = IOTables.digTbl[0][i];
                    }
                   
                    spi_exchange(IOTables.anSize + IOTables.digSize);

                    for (int i=0; i<IOTables.anSize; i++)
                        IOTables.anTbl[1][i] = recvbuf[i];
                    for (int i=0; i<IOTables.digSize; i++)
                        IOTables.digTbl[1][i] = recvbuf[IOTables.anSize + i];

                    tiempoFin = esp_timer_get_time();
                    tiempoTranscurrido = (tiempoFin - tiempoInicio) / 1000;
                    printf("El tiempo de ejecución fue de %llu milisegundos\n", tiempoTranscurrido);

                    tablePrint( IOTables.anTbl[0],  IOTables.anSize);
                    tablePrint( IOTables.anTbl[1],  IOTables.anSize);
                    tablePrint( IOTables.digTbl[0],  IOTables.digSize);
                    tablePrint( IOTables.digTbl[1],  IOTables.digSize);
                    break;

                case 18:             // Test and diagnose case (Echo)
                    sendbuf[0] = recvbuf[0];
                    sendbuf[1] = recvbuf[1];
                    sendbuf[2] = recvbuf[2];
                    sendbuf[3] = recvbuf[3];
                    spi_write(sendbuf, 4);
                    break;

                default:            //Command not recognized
                    sendbuf[0] = 0xFFFF;
                    spi_write(sendbuf, 1);
                    ESP_LOGE(TAG, "Command not recognized! %u\n", sendbuf[0]);
                    SPI_ERROR_COUNT++;
            }
        }
        else{                       //Communication CRC16 error - Bad checksum
            sendbuf[0] = 0xFFFF;
            spi_write(sendbuf, 1);
            ESP_LOGE(TAG, "Communication CRC16 error - Bad checksum!");
            SPI_ERROR_COUNT++;
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
        if (SPI_ERROR_COUNT > 10)
            esp_restart();
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
