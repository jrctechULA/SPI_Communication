/*
___________________________________________________________________________________
    Trans-receptor SPI:
___________________________________________________________________________________
    Este programa configura el puerto SPI2 de un ESP32 en modo esclavo
    e incluye una función para enviar datos, una para recibir y una para
    enviar y recibir al mismo tiempo (Full Duplex).
___________________________________________________________________________________
    La función init_spi():

    1. Configura la estructura del bus spi, (spi_bus_config_t), definiendo 
    los pines de conexión. 
    
    2. Configura la interfaz de conexión, definiendo el pi CS, el modo y el
    tamaño de la cola

    3. Inicializa la interfaz con las estructuras antes definidas.
___________________________________________________________________________________
    La función spi_write(uint32_t payload):

    Envia un dato de tipo uint32_t por SPI al maestro, usando spi_slave_transmit().
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

#include "driver/spi_slave.h"
#include "esp_log.h"


// Pins in use
#define GPIO_MOSI 12
#define GPIO_MISO 13
#define GPIO_SCLK 15
#define GPIO_CS 14

static const char TAG[] = "SLAVE";

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

static esp_err_t spi_rw(uint32_t payload)
{
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 32;
    t.tx_buffer = &payload;
    t.rx_buffer = recvbuf;
    spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
    ESP_LOGI(TAG, "Recv: %li Transm: %li", recvbuf[0], payload);
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
        //spi_write(counter);
        spi_rw(counter);
        counter++;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
