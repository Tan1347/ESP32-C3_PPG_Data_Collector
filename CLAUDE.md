# CLAUDE.md — AI 协作指南

## 项目背景

ESP32-C3 PPG 信号采集固件，使用 ESP-IDF v6.0.1 开发。

## 关键约束

1. **编译环境**: Ubuntu 24.04，ESP-IDF v6.0.1 安装于 `~/esp/v6.0.1/esp-idf`
2. **无 ULP 协处理器**: ESP32-C3 没有 ULP，Deep-sleep 只能用 GPIO0-GPIO5 唤醒
3. **Deep-sleep 唤醒**: 使用 GPIO5 (MAX30102_INT)
4. **代码必须英文**: 所有日志、注释、变量名必须使用英文

## 栈溢出约束

picolibc `vfprintf` 内部使用约 8KB 栈空间。在 32KB 主任务栈上：

**安全**:
- `puts("message")` — 不经过 vsnprintf
- `PPG_LOGI(TAG, func, line, "msg")` — 静态缓冲

**危险**:
- `printf("format %d", value)` — 调用 vsnprintf
- `esp_log_set_vprintf(handler)` — 所有 ESP_LOG 走 vsnprintf

## 编译命令

```bash
cd esp32c3
source ~/esp/v6.0.1/esp-idf/export.sh
bash build.sh
```

## GPIO 分配

| GPIO | 功能 | 说明 |
|------|------|------|
| 0 | 32K晶振 | XTAL_32K_P |
| 1 | 32K晶振 | XTAL_32K_N |
| 2 | SD_SPI_CLK | TF卡时钟 |
| 3 | SD_SPI_MOSI | TF卡数据 |
| 4 | BATT_ADC | 电池电压 |
| 5 | MAX_INT | MAX30102 中断（Deep-sleep 唤醒） |
| 6 | DHT11 | 温湿度传感器 |
| 7 | SD_SPI_CS | TF卡片选 |
| 8 | Card_CD | TF卡检测 |
| 9 | BOOT | 启动按钮 |
| 10 | SD_SPI_MISO | TF卡数据 |
| 11 | BUTTON1 | 用户按钮 |
| 12 | PPG_LED | 采集状态灯（硬件固定） |
| 13 | SYS_LED | 系统状态灯（硬件固定） |
| 14-17 | EXT_FLASH | 外挂Flash（硬件占用） |
| 18 | I2C_SCL | MAX30102时钟 |
| 19 | I2C_SDA | MAX30102数据 |
| 20 | UART0_RX | 调试串口 |
| 21 | UART0_TX | 调试串口 |

## 宏开关

```c
// ppg_config.h
#define BLE_DEBUG_ENABLE    1   // BLE 调试日志 (0=关闭, 1=开启)
#define BATTERY_CHECK_ENABLE 0  // 电池低电检查 (0=禁用, 1=启用)
```

## BLE 帧协议

```
[0xAA][CMD][LEN][DATA...][CHECKSUM]
CHECKSUM = SUM(CMD + LEN + DATA) & 0xFF
```

## SPI 时钟策略

- TF 卡初始化：400kHz
- 正常工作：20MHz
