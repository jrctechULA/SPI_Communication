/*
______________________________________________________________________________________________________
                                        SPI_IO_Slave Library
                                        by: Javier Ruzzante C
                                            (July 2023)
    ______________________________________________________________________________________________________
*/

#ifndef _SPI_IO_Slave
#define _SPI_IO_Slave

#include "driver/spi_slave.h"
#include "esp_log.h"

// Pins in use
#define GPIO_MOSI 12
#define GPIO_MISO 13
#define GPIO_SCLK 15
#define GPIO_CS 14


extern uint32_t recvbuf[];
extern uint32_t sendbuf[];

esp_err_t init_spi(void);
esp_err_t spi_receive(int nData);
esp_err_t spi_write(uint32_t *payload, int nData);

#endif