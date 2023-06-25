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

static const char TAG[] = "SPI";

uint32_t recvbuf[32]={0};
uint32_t sendbuf[32]={0};


static esp_err_t init_spi(void) 
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
    return ESP_OK;
}

static esp_err_t spi_receive(int length)
{
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = length * 8 + 8;
    t.rx_buffer = recvbuf;
    esp_err_t res = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    if (res == ESP_OK)
        ESP_LOGE(TAG, "Received %li", recvbuf[0]);
    return ESP_OK;
} 
static esp_err_t spi_write(uint32_t payload) 
{
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 32;
    t.tx_buffer = &payload;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    ESP_LOGI(TAG, "Transmitted %li", payload);
    return ESP_OK;
}


void app_main(void)
{
    int32_t counter= 10;
    init_spi();
    while (1)
    {
        //spi_receive(4);
        //sendbuf[0] = counter;
        spi_write(counter);
        counter++;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
