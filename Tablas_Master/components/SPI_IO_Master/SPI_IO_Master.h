/*
______________________________________________________________________________________________________
                                        SPI_IO_Master Library
                                        by: Javier Ruzzante C
                                            (July 2023)
    ______________________________________________________________________________________________________
*/

#ifndef _SPI_IO_Master
#define _SPI_IO_Master

#include "driver/spi_master.h"
#include "esp_log.h"

// Pins in use
#define GPIO_MOSI 4
#define GPIO_MISO 5
#define GPIO_SCLK 6
#define GPIO_CS 7

#define SPI_USE_POLLING     //Comment out this line for interrupt transactions

extern spi_device_handle_t handle;
extern uint32_t recvbuf[];
extern uint32_t sendbuf[];

esp_err_t init_spi(void);
esp_err_t spi_write(uint32_t *payload, int nData);
esp_err_t spi_receive(int nData);

#endif