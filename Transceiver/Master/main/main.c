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

static const char TAG[] = "SPI";

uint32_t recvbuf[32]={0};

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
        .clock_speed_hz = SPI_MASTER_FREQ_8M,   //10MHz
        .duty_cycle_pos = 128, // 50% duty cycle
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .cs_ena_posttrans = 0, // Keep the CS low 3 cycles after transaction
        .queue_size = 3};


    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    return ESP_OK;
}

static esp_err_t spi_write(uint32_t payload) 
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 32;
    t.tx_buffer = &payload;
    spi_device_transmit(handle, &t);
    ESP_LOGI(TAG, "Transmitted %li", payload);
    return ESP_OK;
}

static esp_err_t spi_receive(int length)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = length * 8;
    t.rx_buffer = recvbuf;
    esp_err_t res = spi_device_transmit(handle, &t);   
    if (res == ESP_OK)
        ESP_LOGE(TAG, "Received %li", recvbuf[0]);
    return ESP_OK;
} 

// Main application
void app_main(void)
{
    int counter = 0;
    init_spi();
    while (1)
    {
        //spi_write(counter);
        //counter++;

        spi_receive(4);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
