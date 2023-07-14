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
#include "driver/gpio.h"
#include "esp_crc.h"
#include "esp_log.h"

// Pins in use
#define GPIO_MOSI 12
#define GPIO_MISO 13
#define GPIO_SCLK 15
#define GPIO_CS 14
#define GPIO_HANDSHAKE 32

extern uint16_t* recvbuf;
extern uint16_t* sendbuf;

esp_err_t init_spi(void);
esp_err_t spi_receive(uint8_t nData);
esp_err_t spi_write(uint16_t *payload, uint8_t nData);
esp_err_t spi_exchange(uint8_t nData);
uint16_t checksumTable(uint16_t *table, uint8_t nData);


#endif