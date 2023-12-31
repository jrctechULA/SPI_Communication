/*
___________________________________________________________________________________
    Trans-receptor SPI:
___________________________________________________________________________________
    Este programa configura el puerto SPI2 de un ESP32-S3 en modo maestro
    e incluye una función para enviar datos, una para recibir y una para
    enviar y recibir al mismo tiempo (Full Duplex).
___________________________________________________________________________________
    La función init_spi():

    1. Configura la estructura del bus spi, (spi_bus_config_t), definiendo 
    los pines de conexión. 
    
    2. Configura el dispositivo esclavo mediante la estructura 
    spi_device_interface_config_t, definiendo los parámetros como la linea CS, la
    velocidad de transmisión, el tamaño de la cola, el modo y el ciclo de servicio.

    3. Inicializa el bus y agrega el esclavo identificandolo con el handle 
    previamente declarado (spi_device_handle_t)
___________________________________________________________________________________
    La función spi_write(uint32_t payload):

    Envia un dato de tipo uint32_t por SPI al esclavo, usando spi_device_transmit().
___________________________________________________________________________________

    La funcion spi_receive(int length):

    Recibe el número de datos de tipo uint32_t especificado en length, los datos se
    almacenan en el array recvbuf. (La función puede recibir hasta 32 valores, sin
    embargo, el log que se imprime sólo muestra recvbuf[0]).
___________________________________________________________________________________
    La función spi_rw(uint32_t payload):

    Combina ambas funciones anteriores en una sola transacción, ya que se definen
    t.tx_buffer y t.rx_buffer, implementa una comunicación full duplex.
    A diferencia de spi_receive(), sólo se recibe un dato, dado que t.length es 32.
    ___________________________________________________________________________________

*/

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

static const char TAG[] = "MASTER";

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

static esp_err_t spi_rw(uint32_t payload)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 32;
    t.tx_buffer = &payload;
    t.rx_buffer = recvbuf;
    esp_err_t res = spi_device_transmit(handle, &t);   
    ESP_LOGE(TAG, "Recv: %li Transm: %li", recvbuf[0], payload);
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
        //spi_receive(4);

        counter++;
        spi_rw(counter);

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
