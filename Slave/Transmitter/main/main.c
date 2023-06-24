// Slave as a transmitter for SPI communitation

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

int i; // Counter

void app_main(void)
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

    // SPI variables 
    char sendbuf[128] = {0};
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    
    printf("Slave transmission:\n");
    while (1)
    {
        memset(sendbuf, 0, sizeof(sendbuf));
        snprintf(sendbuf, sizeof(sendbuf), "Sent by slave - %d", i);
        t.length = sizeof(sendbuf) * 8;
        t.tx_buffer = sendbuf;
        spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
        printf("Transmitted: %s\n", sendbuf);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        i++;
    }
}
