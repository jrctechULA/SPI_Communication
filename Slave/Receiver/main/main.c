// Slave as receiver for SPI communitation

#include <stdio.h>
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

//Main application
void app_main(void)
{
    //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_MOSI,
        .miso_io_num=GPIO_MISO,
        .sclk_io_num=GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg={
        .mode=0,
        .spics_io_num=GPIO_CS,
        .queue_size=1,
        .flags=0,
    };

    //Initialize SPI slave interface
    spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);

    char recvbuf[129]="";
    memset(recvbuf, 0, 33);
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

	printf("Slave output:\n");
    while(1) {
        t.length=128*8;
        t.rx_buffer=recvbuf;
        spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
        printf("Received: %s\n", recvbuf);
    }
}

