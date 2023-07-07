//____________________________________________________________________________________________________
// Include section:
//____________________________________________________________________________________________________
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_slave.h"
#include "SPI_IO_Slave.h"
#include "esp_log.h"

//____________________________________________________________________________________________________
// Macro definitions:
//____________________________________________________________________________________________________


//____________________________________________________________________________________________________
// Global declarations:
//____________________________________________________________________________________________________
DMA_ATTR uint32_t recvbuf[32]={0};
DMA_ATTR uint32_t sendbuf[32]={0};

typedef struct {
	int** analogTables;   // Vector de apuntadores a los vectores analógicos
    int** digitalTables;  // Vector de apuntadores a los vectores Digitales
	int analogSize;       // Tamaño de los vectores analógicos
    int digitalSize;      // Tamaño de los vectores analógicos
	int numAnTables;      // Número de vectores analógicos
    int numDigTables;     //Número de vectores digitales
} varTables_t;

//____________________________________________________________________________________________________
// Function prototypes:
//____________________________________________________________________________________________________

esp_err_t tablesInit(varTables_t *tables, int numAnTables, int numDigTables, int analogSize, int digitalSize);
esp_err_t tablesPrint(varTables_t *tables);
esp_err_t tablesUnload(varTables_t *tables);

//____________________________________________________________________________________________________
// Main program:
//____________________________________________________________________________________________________
void app_main(void)
{
    static const char TAG[] = "Slave Main";
    
    varTables_t IOTables;
    ESP_LOGI(TAG, "Tamaño del objeto: %i bytes\n", sizeof(IOTables));  //Imprime el tamaño de la estructura, el cual es constante independientemente del número y tamaño de los vectores
    tablesInit(&IOTables, 3,2,10,3);

    tablesPrint(&IOTables);
    uint32_t tbl, payload;
    int dataIndex;
    
    init_spi();
    while (1)
    {
        // Check request:
        memset(recvbuf, 0, sizeof(recvbuf));
        spi_receive(4);
        switch (recvbuf[0])
        {
            case 1:             //Request for analog table
                printf("Requested analog table %li\n", recvbuf[1]);
                printf("%i %i %i %i %i %i %i %i %i %i\n", IOTables.analogTables[recvbuf[1]][0], IOTables.analogTables[recvbuf[1]][1], IOTables.analogTables[recvbuf[1]][2], IOTables.analogTables[recvbuf[1]][3], IOTables.analogTables[recvbuf[1]][4], IOTables.analogTables[recvbuf[1]][5], IOTables.analogTables[recvbuf[1]][6], IOTables.analogTables[recvbuf[1]][7], IOTables.analogTables[recvbuf[1]][8], IOTables.analogTables[recvbuf[1]][9]);
                spi_write(IOTables.analogTables[recvbuf[1]], IOTables.analogSize);
                break;

            case 2:             //Request for digital table
                printf("Requested digital table %li\n", recvbuf[1]);
                printf("%i %i %i\n", IOTables.digitalTables[recvbuf[1]][0], IOTables.digitalTables[recvbuf[1]][1], IOTables.digitalTables[recvbuf[1]][2]);
                spi_write(IOTables.digitalTables[recvbuf[1]], IOTables.digitalSize);
                break;

            case 3:             //Request for single analog value
                printf("Requested value %li from analog table %li\n", recvbuf[2], recvbuf[1]);
                printf("%i\n", IOTables.analogTables[recvbuf[1]][recvbuf[2]]);
                spi_write(&IOTables.analogTables[recvbuf[1]][recvbuf[2]], 1);
                break;
            case 4:             //Request for single digital value
                printf("Requested value %li from digital table %li\n", recvbuf[2], recvbuf[1]);
                printf("%i\n", IOTables.digitalTables[recvbuf[1]][recvbuf[2]]);
                spi_write(&IOTables.digitalTables[recvbuf[1]][recvbuf[2]], 1);
                break;

            case 5:             //Request for write analog table
                tbl = recvbuf[1];
                printf("Write request to analog table %li\n", tbl);
                spi_receive(IOTables.analogSize);
                //vTaskDelay(pdMS_TO_TICKS(10));
                
                for (int j=0; j < IOTables.analogSize; j++){
                    IOTables.analogTables[tbl][j] = recvbuf[j];
                    printf("%lu ", (uint32_t)IOTables.analogTables[tbl][j]);
                    recvbuf[j]=0;
                }
                printf("\n");
                
                break;

                case 6:             //Request for write digital table
                    tbl = recvbuf[1];
                    printf("Write request to digital table %li\n", tbl);
                    spi_receive(IOTables.digitalSize);
                    //vTaskDelay(pdMS_TO_TICKS(10));
                    
                    for (int j=0; j < IOTables.digitalSize; j++){
                        IOTables.digitalTables[tbl][j] = recvbuf[j];
                        printf("%lu ", (uint32_t)IOTables.digitalTables[tbl][j]);
                        recvbuf[j]=0;
                    }
                    printf("\n");
                    break;
                case 7:             //Request for write single analog data
                    tbl = recvbuf[1];
                    dataIndex = recvbuf[2];
                    payload = recvbuf[3];
                    IOTables.analogTables[tbl][dataIndex] = payload;
                    printf("Written %lu to analog table %lu at index %i\n", (uint32_t)IOTables.analogTables[tbl][dataIndex], tbl, dataIndex);
                    break;
                
                case 8:             //Request for write single digital data
                    tbl = recvbuf[1];
                    dataIndex = recvbuf[2];
                    payload = recvbuf[3];
                    IOTables.digitalTables[tbl][dataIndex] = payload;
                    printf("Written %lu to digital table %lu at index %i\n", (uint32_t)IOTables.digitalTables[tbl][dataIndex], tbl, dataIndex);
                    break;

                break;

            case 0:             //Communication error
                sendbuf[0] = 0xFFFFFFFF;
                spi_write(sendbuf, 1);
                printf("Communication error!\n");
                break;

            default:            //Command not recognized
                sendbuf[0] = 0xFFFFFFFF;
                spi_write(sendbuf, 1);
                printf("Command not recognized!\n");
        }
        
        //Actualizar los vectores analógicos:
        //ESP_LOGI(TAG, "Actualizando vectores analógicos... %i vectores, Tamaño: %i", IOTables.numAnTables, IOTables.analogSize);
        for (int i=0; i<IOTables.numAnTables; i++){
            for (int j=0; j<IOTables.analogSize; j++){		
                IOTables.analogTables[i][j] += 1;
            }
        }

        //Actualizar los vectores digitales:
        //ESP_LOGI(TAG, "Actualizando vectores digitales... %i vectores, Tamaño: %i", IOTables.numDigTables, IOTables.digitalSize);
        for (int i=0; i<IOTables.numDigTables; i++){
            for (int j=0; j<IOTables.digitalSize; j++){		
                IOTables.digitalTables[i][j] += 1;
            }
        }
        //ESP_LOGI(TAG, "Tablas cargadas\n\n");

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    //Free memory
	//tablesUnload(&IOTables);
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