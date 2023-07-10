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

esp_err_t spi_receive(uint8_t nData)
{
    static const char TAG[] = "SPI Slave";
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    if ((nData * 2) % 4)                //Data must be 32bit aligned!
        t.length = nData * 16 + 48;
    else
        t.length = nData * 16 + 32;
    t.rx_buffer = recvbuf;
    t.tx_buffer = NULL;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    //if (res == ESP_OK)
        ESP_LOGW(TAG, "Received %u %u %u %u %u", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3], recvbuf[4]);
    return ESP_OK;
} 

esp_err_t spi_write(uint16_t *payload, uint8_t nData) 
{
    //static const char TAG[] = "SPI Slave";
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    if ((nData * 2) % 4)                //Data must be 32bit aligned!
        t.length = nData * 16 + 16;
    else
        t.length = nData * 16;
    t.tx_buffer = payload;
    t.rx_buffer = NULL;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    //ESP_LOGI(TAG, "Transmitted %u %u %u %u %u %u %u %u %u %u ", payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
    return ESP_OK;
}