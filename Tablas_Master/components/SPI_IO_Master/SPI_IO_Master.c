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
        .cs_ena_posttrans = 3, // Keep the CS low 0 cycles after transaction
        .queue_size = 5};


    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &handle);

    //spi_device_acquire_bus(handle, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t spi_write(uint16_t *payload, uint8_t nData) 
{
    static const char TAG[] = "SPI Master";
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    if ((nData * 2) % 4)                //Data must be 32bit aligned!
        t.length = nData * 16 + 16;
    else
        t.length = nData * 16;
    t.tx_buffer = payload;
    t.rx_buffer = NULL;
    #ifdef SPI_USE_POLLING
        //ESP_LOGW(TAG, "Polling transactions activated!");
        spi_device_polling_transmit(handle, &t);
    #else
       spi_device_transmit(handle, &t);
    #endif
    ESP_LOGI(TAG, "Transmitted: %u %u %u %u %u", payload[0], payload[1], payload[2], payload[3], payload[4]);
    return ESP_OK;
}

esp_err_t spi_receive(uint8_t nData)
{
    //static const char TAG[] = "SPI Master";
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    if ((nData * 2) % 4)                //Data must be 32bit aligned!
        t.length = nData * 16 + 16;
    else
        t.length = nData * 16;
    t.tx_buffer = NULL;
    t.rx_buffer = recvbuf;
    #ifdef SPI_USE_POLLING
        //ESP_LOGW(TAG, "Polling transactions activated!");
        spi_device_polling_transmit(handle, &t);
    #else
       spi_device_transmit(handle, &t);
    #endif
    return ESP_OK;
} 