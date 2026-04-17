#include "wifi_connect.h"



/* FreeRTOS事件组句柄 - 用于通知WiFi连接状态 */
static EventGroupHandle_t s_wifi_event_group;

/* 事件组位定义
   事件组允许多个位表示不同事件，我们只关心两个事件：
   - 已连接到AP并获取到IP地址
   - 达到最大重试次数后连接失败
*/
#define WIFI_CONNECTED_BIT BIT0  // 连接成功标志位
#define WIFI_FAIL_BIT      BIT1  // 连接失败标志位

/* 日志标签 - 用于在日志输出中标识此模块 */
static const char *TAG = "wifi station";

/* 重试计数器 - 记录当前已重试连接的次数 */
static int s_retry_num = 0;

/**
 * @brief WiFi和IP事件处理函数
 *
 * 处理WiFi连接过程中的各种事件：
 * - WIFI_EVENT_STA_START: WiFi站点模式启动，开始连接
 * - WIFI_EVENT_STA_DISCONNECTED: 断开连接，尝试重连或标记失败
 * - IP_EVENT_STA_GOT_IP: 成功获取IP地址，标记连接成功
 *
 * @param arg 用户自定义参数（未使用）
 * @param event_base 事件基类型（WIFI_EVENT或IP_EVENT）
 * @param event_id 具体事件ID
 * @param event_data 事件数据指针
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    // WiFi站点模式启动事件
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();  // 启动WiFi连接
    }
    // WiFi断开连接事件
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            // 未达到最大重试次数，尝试重新连接
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            // 已达到最大重试次数，设置失败标志位
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    }
    // 成功获取IP地址事件
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        // 打印获取到的IP地址
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;  // 重置重试计数器
        // 设置连接成功标志位
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 初始化WiFi站点模式
 *
 * 执行以下操作：
 * 1. 创建FreeRTOS事件组
 * 2. 初始化网络接口(netif)
 * 3. 创建默认事件循环
 * 4. 注册WiFi和IP事件处理函数
 * 5. 配置并启动WiFi连接
 * 6. 等待连接结果（成功或失败）
 */
void wifi_init_sta(void *arg)
{
    /* 创建FreeRTOS事件组，用于同步WiFi连接状态 */
    s_wifi_event_group = xEventGroupCreate();

    /* 初始化底层TCP/IP协议栈 */
    ESP_ERROR_CHECK(esp_netif_init());

    /* 创建默认事件循环，用于处理系统事件 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 创建默认的WiFi站点网络接口
       注意：调用此函数之前必须先调用esp_netif_init() */
    esp_netif_create_default_wifi_sta();

    /* 使用默认配置初始化WiFi驱动 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 注册事件处理函数实例 */
    esp_event_handler_instance_t instance_any_id;   // 处理所有WiFi事件
    esp_event_handler_instance_t instance_got_ip;   // 处理获取IP事件

    /* 注册WiFi事件处理函数 - 处理所有WiFi事件类型 */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    /* 注册IP事件处理函数 - 仅处理获取IP事件 */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    /* 配置WiFi连接参数 */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,           // WiFi名称
            .password = EXAMPLE_ESP_WIFI_PASS,       // WiFi密码
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    /* 设置WiFi为站点模式 */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* 设置WiFi配置参数 */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    /* 启动WiFi驱动 */
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* 等待连接结果：连接成功(WIFI_CONNECTED_BIT)或达到最大重试次数失败(WIFI_FAIL_BIT)
     * 这些标志位由event_handler()函数设置 */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,  // 等待的位
            pdFALSE,                              // 不清除位
            pdFALSE,                              // 不等待所有位（任一位触发即可）
            portMAX_DELAY);                       // 无限等待

    /* 根据返回的位判断最终连接结果 */
    if (bits & WIFI_CONNECTED_BIT) {
        // 连接成功
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        // 连接失败
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        // 未知事件（理论上不应该发生）
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    vTaskDelete(NULL);
}

/**
 * @brief 应用程序入口函数
 *
 * 执行以下操作：
 * 1. 初始化NVS（非易失性存储）
 * 2. 配置WiFi模块日志级别
 * 3. 启动WiFi站点模式
 */

