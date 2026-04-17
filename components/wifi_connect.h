/* WiFi station Example - WiFi站点模式示例

   本示例代码属于公共领域（或根据您的选择采用CC0许可）

   除非适用法律要求或书面同意，否则本软件按"原样"分发，
   不附带任何明示或暗示的担保或条件。
*/
#include <string.h>
#include "freertos/FreeRTOS.h"      // FreeRTOS核心头文件
#include "freertos/task.h"          // FreeRTOS任务管理
#include "freertos/event_groups.h"  // FreeRTOS事件组
#include "esp_system.h"             // ESP32系统接口
#include "esp_wifi.h"               // WiFi驱动接口
#include "esp_event.h"              // 事件循环接口
#include "esp_log.h"                // 日志系统
#include "nvs_flash.h"              // 非易失性存储Flash接口

#include "lwip/err.h"               // LwIP错误码定义
#include "lwip/sys.h"               // LwIP系统抽象层

/* WiFi配置参数 - 可通过项目配置菜单(menuconfig)设置
   如果不想使用menuconfig，可以直接修改下面的定义为字符串
   例如: #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "ESP-01"           // WiFi名称(SSID)
#define EXAMPLE_ESP_WIFI_PASS      "shimu123"       // WiFi密码
#define EXAMPLE_ESP_MAXIMUM_RETRY  3       // 最大重连次数

/* WPA3 SAE(同步认证等价)模式配置
   SAE_PWE: 密码元素选择方式
   - HUNT_AND_PECK: 传统方式，逐个尝试
   - HASH_TO_ELEMENT: 哈希到元素方式，更高效
   - BOTH: 两种方式都支持
*/
#if CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

/* WiFi认证模式阈值配置
   设置连接WiFi时要求的最低安全等级
   只有AP的认证模式不低于此阈值才会连接
*/
#if 1
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN           // 开放认证（无密码）
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP            // WEP加密（已不安全）
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK        // WPA-PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK       // WPA2-PSK（最常用）
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK   // WPA/WPA2混合模式
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK       // WPA3-PSK（最新标准）
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK  // WPA2/WPA3混合模式
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK       // WAPI（中国标准）
#endif
void wifi_init_sta(void *arg);