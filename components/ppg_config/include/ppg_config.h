/**
 * @file ppg_config.h
 * @brief 全局配置 — 引脚、常量、版本号
 */

#pragma once

#include <stdint.h>

/* ==================== 固件版本 ==================== */
#define PPG_FW_VERSION          "1.0.0"
#include "version_ts.h"

/* ==================== 系统状态 ==================== */
typedef enum {
    STATE_DEEP_SLEEP = 0,
    STATE_LIGHT_SLEEP,
    STATE_MEASURING,
    STATE_BLE_CONNECTED,
    STATE_BLE_PAIRING,      /* BLE 配对模式（长按 BOOT 进入） */
    STATE_WIFI_STA,
    STATE_OTA,
    STATE_WIFI_SHUTDOWN,
    STATE_STANDALONE,       /* 独立采集模式（关闭 WiFi/BLE，只采集存卡） */
} system_state_t;

/* ==================== I2C 引脚 ==================== */
#define PPG_I2C_PORT            I2C_NUM_0
#define PPG_I2C_SDA_PIN         GPIO_NUM_19
#define PPG_I2C_SCL_PIN         GPIO_NUM_18
#define PPG_I2C_FREQ_HZ        400000  /* 400kHz Fast-mode */

/* ==================== MAX30102 ==================== */
#define MAX30102_I2C_ADDR       0x57
#define MAX30102_INT_PIN        GPIO_NUM_5  /* MAX30102 中断引脚 (Deep-sleep 唤醒) */
#define PPG_SAMPLE_RATE_HZ      100     /* 100Hz 采样率 */
#define PPG_SAMPLE_INTERVAL_MS  (1000 / PPG_SAMPLE_RATE_HZ)

/* ==================== ADC 电池检测 ==================== */
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_4   /* GPIO4 */
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12
#define BATTERY_DIVIDER_RATIO   2       /* 100K + 100K 分压 */
#define BATTERY_FILTER_CAP_NF   100     /* 并联 100nF 滤波电容 */
#define BATTERY_SAMPLE_COUNT    64      /* 64 次均值 */
#define BATTERY_LOW_VOLTAGE     330     /* 3.3V 低电阈值 */
#define BATTERY_SHUTDOWN_VOLTAGE 320    /* 3.2V 强制关机 */
#define BATTERY_WIFI_MIN_SOC    20      /* WiFi 最低电量 20% */
#define BATTERY_PPG_MIN_SOC     10      /* PPG 采集最低电量 10% */

/* Battery check switch: 1=enable, 0=disable (for debugging without battery) */
#define BATTERY_CHECK_ENABLE    0

/* ==================== DHT11 温湿度传感器 ==================== */
#define DHT11_GPIO_PIN              GPIO_NUM_6  /* DHT11 数据引脚 */

/* ==================== 按钮 ==================== */
#define BOOT_BUTTON_GPIO            GPIO_NUM_9  /* BOOT 按钮 (低电平=按下) */
#define BUTTON1_GPIO                GPIO_NUM_2  /* 按钮1 (低电平=按下) */

/* ==================== Deep-sleep 唤醒 ==================== */
/* ESP32-C3 Deep-sleep GPIO 唤醒仅支持 GPIO0-GPIO5 */
/* MAX30102_INT_PIN (GPIO5) 用于唤醒，手指放上传感器自动唤醒 */

/* ==================== LED ==================== */
#define PPG_LED_PIN                 GPIO_NUM_12 /* PPG 采集状态灯 */
#define SYS_LED_PIN                 GPIO_NUM_13 /* 系统工作状态灯 (1秒闪) */

/* ==================== TF 卡 SPI ==================== */
#define SD_SPI_HOST             SPI2_HOST
#define SD_SPI_CS_PIN           GPIO_NUM_7
#define SD_SPI_CLK_PIN          GPIO_NUM_11
#define SD_SPI_MOSI_PIN         GPIO_NUM_3
#define SD_SPI_MISO_PIN         GPIO_NUM_10
/* GPIO8 is a strapping pin - do NOT use for SD_CARD_CD to avoid boot issues */
#define SD_MOUNT_POINT          "/sdcard"

/* ==================== BLE ==================== */
#define BLE_DEVICE_NAME         "PPG-Monitor"
#define BLE_SVC_UUID            0xFFF0
#define BLE_CHAR_STATUS_UUID    0xFFF1
#define BLE_CHAR_LIVE_UUID      0xFFF2
#define BLE_CHAR_CMD_UUID       0xFFF3
#define BLE_CHAR_FILELIST_UUID  0xFFF4

/* BLE 命令定义 */
#define BLE_CMD_START_MEASURE   0x01
#define BLE_CMD_STOP_MEASURE    0x02
#define BLE_CMD_START_WIFI      0x03
#define BLE_CMD_WIFI_ADD        0x10
#define BLE_CMD_WIFI_STATUS     0x11
#define BLE_CMD_WIFI_CLEAR      0x12
#define BLE_CMD_WIFI_DELETE     0x13
#define BLE_CMD_WIFI_LIST       0x14
#define BLE_CMD_OTA_ENTER       0x20
#define BLE_CMD_FW_VERSION      0x21
#define BLE_CMD_QUERY_STATUS    0x22    /* Query full status (battery, version, WiFi) */
#define BLE_CMD_QUERY_SD_CARD   0x23    /* Query SD card capacity */
#define BLE_CMD_QUERY_BATTERY   0x24    /* Query battery details */
#define BLE_CMD_TIME_SYNC       0x40    /* 时间同步（App 发送 Unix10 时间戳） */
#define BLE_CMD_STANDALONE      0x41    /* 进入独立采集模式 */
#define BLE_CMD_UART_RECORD     0x50    /* 串口记录控制 */
#define BLE_CMD_LOG_LEVEL       0x30
#define BLE_CMD_LOG_STATUS      0x31
#define BLE_CMD_FILE_DOWNLOAD   0x32    /* BLE trigger + HTTP download */

/* ==================== WiFi ==================== */
#define WIFI_MAX_CREDENTIALS    5
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_HTTP_TIMEOUT_SEC   120

/* ==================== 日志系统 ==================== */
#define LOG_RING_BUFFER_SIZE    (4 * 1024)  /* 4KB 环形缓冲 (节省内存) */
#define LOG_FLUSH_THRESHOLD     (LOG_RING_BUFFER_SIZE / 2)
#define LOG_FLUSH_INTERVAL_MS   30000       /* 30 秒定时刷写 */
#define LOG_MAX_FILE_SIZE       (10 * 1024 * 1024)  /* 10MB 单文件上限 */
#define LOG_MAX_FILES           5           /* 保留最近 5 个文件 */

/* UART 调试开关 */
#ifndef PPG_LOG_UART_ENABLE
#define PPG_LOG_UART_ENABLE     1           /* 1=开启 UART 输出，0=关闭 */
#endif

/* ==================== 电源管理 ==================== */
#define PM_MAX_FREQ_MHZ         160
#define PM_MIN_FREQ_MHZ         10
#define PM_LIGHT_SLEEP_ENABLE   true

/* ==================== OTA ==================== */
#define OTA_BUFFER_SIZE         4096
#define OTA_ROLLBACK_ENABLE     true

/* ==================== Task Stack Sizes ==================== */
#define STACK_PPG_TASK          4096
#define STACK_BLE_CMD           4096
#define STACK_SYS_LED           4096
#define STACK_PPG_LED           3072
#define STACK_BUTTON1           3072
#define STACK_POWER             3072
#define STACK_DHT11             3072

/* ==================== Timeouts (ms) ==================== */
#define TIMEOUT_BLE_PAIR_WAKEUP     30000
#define TIMEOUT_BLE_PAIR_COLDBOOT   60000
#define TIMEOUT_WIFI_CONNECT        30000
#define TIMEOUT_HTTP_FETCH          5000
#define TIMEOUT_WIFI_IDLE           60000
#define TIMEOUT_STANDBY_AWAKE       30000
#define TIMEOUT_DEEP_SLEEP_NO_INT   300000
#define TIMEOUT_WDT                 5000
#define TIMEOUT_BUTTON_LONG_PRESS   3000
#define TIMEOUT_BLE_CMD_DELAY       500

/* ==================== LED Blink ==================== */
#define PPG_LED_RATE_OFF        30
#define PPG_LED_RATE_SLOW       70
#define PPG_LED_RATE_FAST       100
#define PPG_LED_BLINK_SLOW_MS   600
#define PPG_LED_BLINK_MED_MS    300
#define PPG_LED_BLINK_FAST_MS   150
#define PPG_LED_BLINK_MAX_MS    80

/* ==================== Validation ==================== */
#define MIN_VALID_TIMESTAMP     1700000000
#define BATTERY_MIN_WIFI_PCT    20
#define BATTERY_MIN_PPG_PCT     10
#define BLE_QUALITY_VALID       80
#define BLE_QUALITY_INVALID     20

/* ==================== System State API ==================== */
void system_set_state(system_state_t new_state);
system_state_t system_get_state(void);

