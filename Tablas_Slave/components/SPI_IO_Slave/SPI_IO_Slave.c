/*
______________________________________________________________________________________________________
                                        SPI_IO_Slave Library
                                        by: Javier Ruzzante C
                                            (July 2023)
    ______________________________________________________________________________________________________
*/

#include "SPI_IO_Slave.h"
#include "string.h"


void my_post_setup_cb(spi_slave_transaction_t *trans) {
    gpio_set_level(GPIO_HANDSHAKE, 1);
}

//Called after transaction is sent/received. We use this to set the handshake line low.
void my_post_trans_cb(spi_slave_transaction_t *trans) {
    gpio_set_level(GPIO_HANDSHAKE, 0);
}

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
        .post_setup_cb=my_post_setup_cb,
        .post_trans_cb=my_post_trans_cb
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
    if (((nData + 1) * 2) % 4)                //Data must be 32bit aligned!
        t.length = (nData + 1) * 16 + 48;
    else
        t.length = (nData + 1) * 16 + 32;
    t.rx_buffer = recvbuf;
    t.tx_buffer = NULL;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    
    uint16_t crcRecv = recvbuf[nData];
    uint16_t crc = checksumTable(recvbuf, nData);
    if (crcRecv != crc){
        ESP_LOGE("CRC Check:", "Communication CRC16 error - Bad checksum!");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Received %u %u %u %u %u", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3], recvbuf[4]);
    return ESP_OK;
} 

esp_err_t spi_write(uint16_t *payload, uint8_t nData) 
{
    //static const char TAG[] = "SPI Slave";

    payload[nData] = checksumTable(payload, nData);

    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    if (((nData + 1) * 2) % 4)                //Data must be 32bit aligned!
        t.length = (nData + 1) * 16 + 16;
    else
        t.length = (nData + 1) * 16;
    t.tx_buffer = payload;
    t.rx_buffer = NULL;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    //ESP_LOGI(TAG, "Transmitted %u %u %u %u %u %u %u %u %u %u ", payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
    return ESP_OK;
}

esp_err_t spi_exchange(uint8_t nData){
    static const char TAG[] = "SPI Slave";

    sendbuf[nData] = checksumTable(sendbuf, nData);

    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    if (((nData + 1) * 2) % 4)                //Data must be 32bit aligned!
        t.length = (nData + 1) * 16 + 48;
    else
        t.length = (nData + 1) * 16 + 32;
    t.rx_buffer = recvbuf;
    t.tx_buffer = sendbuf;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);

    uint16_t crcRecv = recvbuf[nData];
    uint16_t crc = checksumTable(recvbuf, nData);
    if (crcRecv != crc){
        ESP_LOGE("CRC Check:", "Communication CRC16 error - Bad checksum!");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Received %u %u %u %u %u", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3], recvbuf[4]);
    return ESP_OK;
}

uint16_t checksumTable(uint16_t *table, uint8_t nData){
    //size_t data_len = sizeof(table) / 2;
    uint16_t crc = 0xFFFF; // Valor inicial del CRC-16
    
    for (size_t i = 0; i < nData; i++) {
        uint8_t* bytes = (uint8_t*)&table[i];
        size_t num_bytes = sizeof(table[i]);
        
        for (size_t j = 0; j < num_bytes; j++) {
            //crc = esp_crc16_le(&bytes[j], 1, crc);
            crc = esp_crc16_le(crc, &bytes[j], 1);
        }
    }
    return crc;
}