#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "wifi_AP.h"

static const char *TAG = "uart_tcp_bridge";

#define EX_UART_NUM   UART_NUM_0
#define UART_TX_PIN   GPIO_NUM_43
#define UART_RX_PIN   GPIO_NUM_44
#define BUF_SIZE      (1024)
#define RD_BUF_SIZE   (BUF_SIZE)
#define TCP_PORT      8080

static QueueHandle_t uart0_queue;
static volatile int client_sock = -1;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);

    for (;;) {
        if (xQueueReceive(uart0_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            switch (event.type) {
            case UART_DATA:
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                ESP_LOGI(TAG, "UART -> TCP: %d bytes", event.size);
                int sock = client_sock;
                if (sock >= 0) {
                    send(sock, dtmp, event.size, 0);
                }
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART FIFO overflow");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart0_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART buffer full");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart0_queue);
                break;
            default:
                break;
            }
        }
    }
    free(dtmp);
    vTaskDelete(NULL);
}

static void tcp_server_task(void *pvParameters)
{
    char rx_buffer[BUF_SIZE];

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_family = AF_INET,
        .sin_port = htons(TCP_PORT),
    };

    if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Socket bind failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) != 0) {
        ESP_LOGE(TAG, "Socket listen failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    for (;;) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int new_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (new_sock < 0) {
            ESP_LOGE(TAG, "Accept failed");
            continue;
        }

        ESP_LOGI(TAG, "Client connected: %s:%d",
                 inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port));

        int flag = 1;
        setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        int old_sock = client_sock;
        client_sock = new_sock;
        if (old_sock >= 0) {
            close(old_sock);
        }

        for (;;) {
            int len = recv(new_sock, rx_buffer, BUF_SIZE - 1, 0);
            if (len <= 0) {
                ESP_LOGI(TAG, "Client disconnected");
                if (client_sock == new_sock) {
                    client_sock = -1;
                }
                close(new_sock);
                break;
            }

            ESP_LOGI(TAG, "TCP -> UART: %d bytes", len);
            uart_write_bytes(EX_UART_NUM, rx_buffer, len);
        }
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_ap();

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    uart_set_pin(EX_UART_NUM, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);
    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, 10, NULL);
}
