/*
______________________________________________________________________________________________________
                                        SPI_IO_Slave Library
                                        by: Javier Ruzzante C
                                            (July 2023)
    ______________________________________________________________________________________________________
*/

#include "SPI_IO_Slave.h"
#include "string.h"

esp_err_t init_spi(void) 
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

esp_err_t spi_receive(int nData)
{
    static const char TAG[] = "SPI Slave";
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32 + 32;
    t.rx_buffer = recvbuf;
    esp_err_t res = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    if (res == ESP_OK)
        ESP_LOGE(TAG, "Received %lu %lu %lu %lu", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3]);
    return ESP_OK;
} 

esp_err_t spi_write(uint32_t *payload, int nData) 
{
    //static const char TAG[] = "SPI Slave";
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32;
    t.tx_buffer = payload;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    //ESP_LOGI(TAG, "Transmitted %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu ", payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
    return ESP_OK;
}