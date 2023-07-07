/*
______________________________________________________________________________________________________
                                        SPI_IO_Master Library
                                        by: Javier Ruzzante C
                                            (July 2023)
    ______________________________________________________________________________________________________
*/

#include "SPI_IO_Master.h"
#include "string.h"

esp_err_t init_spi(void) 
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
        .clock_speed_hz = SPI_MASTER_FREQ_8M,   //8MHz
        .duty_cycle_pos = 128, // 50% duty cycle
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .cs_ena_posttrans = 0, // Keep the CS low 0 cycles after transaction
        .queue_size = 3};


    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    return ESP_OK;
}

esp_err_t spi_write(uint32_t *payload, int nData) 
{
    static const char TAG[] = "SPI Master";
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32;
    t.tx_buffer = payload;
    #ifdef SPI_USE_POLLING
        //ESP_LOGW(TAG, "Polling transactions activated!");
        spi_device_polling_transmit(handle, &t);
    #else
       spi_device_transmit(handle, &t);
    #endif
    ESP_LOGI(TAG, "Transmitted: %lu %lu %lu %lu", payload[0], payload[1], payload[2], payload[3]);
    return ESP_OK;
}

esp_err_t spi_receive(int nData)
{
    static const char TAG[] = "SPI Master";
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = nData * 32;
    t.rx_buffer = recvbuf;
    #ifdef SPI_USE_POLLING
        //ESP_LOGW(TAG, "Polling transactions activated!");
        spi_device_polling_transmit(handle, &t);
    #else
       spi_device_transmit(handle, &t);
    #endif
    /* ESP_LOGE(TAG, "Received %lu %lu %lu %lu", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3]); */

    ESP_LOGE(TAG, "Reception completed");
    return ESP_OK;
} 
