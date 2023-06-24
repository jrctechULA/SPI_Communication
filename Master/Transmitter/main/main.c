// Master as transmitter for SPI communication

#include <stdio.h>
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

// Main application
void app_main(void)
{
    spi_device_handle_t handle;

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
        .clock_speed_hz = 5000000,
        .duty_cycle_pos = 128, // 50% duty cycle
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .cs_ena_posttrans = 3, // Keep the CS low 3 cycles after transaction
        .queue_size = 3};

    char sendbuf[128] = {0};

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &handle);

    printf("Master output:\n");
    while (1)
    {
        snprintf(sendbuf, sizeof(sendbuf), "Esto es una prueba de transmisión por SPI desde el Master al Slave");
        t.length = sizeof(sendbuf) * 8;
        t.tx_buffer = sendbuf;
        spi_device_transmit(handle, &t);
        printf("Transmitted: %s\n", sendbuf);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
