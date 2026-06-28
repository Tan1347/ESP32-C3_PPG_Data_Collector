# ESP32-C3 PPG 信号采集固件

> **固件版本**: 1.0.0  
> **编译环境**: Ubuntu 24.04 + ESP-IDF v6.0.1  
> **当前状态**: 编译通过，硬件实测中
> **Flash 配置**: DIO 40MHz（稳定性优先）

---

## 项目概述

基于 ESP-IDF v6.0.1 的 PPG（光电容积脉搏波）信号采集固件，运行于 ESP32-C3 平台。集成 MAX30102 传感器驱动、心率/血氧算法、BLE 通信、WiFi 数据传输、TF 卡本地存储、OTA 升级等功能。

### 核心特性

| 模块 | 说明 |
|------|------|
| MAX30102 | I2C 驱动，100Hz 采样率，中断驱动 |
| PPG 算法 | 纯定点运算，5 秒滑动窗口，峰值检测 + Hamming 滤波 |
| BLE | NimBLE 协议栈，帧协议通信（0xAA 帧头 + SUM 校验） |
| WiFi | Station 模式，HTTP Server，文件传输 + Web 管理 + OTA |
| TF 卡 | FAT32 + LZ4 压缩，主备缓冲 (32KB+8KB)，SPI 400kHz→20MHz |
| UART 录制 | 双缓冲 DMA (2x32KB)，支持 9600~5Mbps 波特率 |
| DHT11 | 温湿度监测，二进制格式存储 |
| OTA | 双分区 A/B，SHA-256 校验，60 秒启动自检，失败自动回滚 |
| 电源 | DFS 动态调频，Auto Light-sleep，Deep-sleep，电池电量保护（可禁用） |

---

## 硬件引脚

| 功能 | GPIO | 说明 |
|------|------|------|
| MAX_INT | GPIO5 | MAX30102 中断输出 (低电平有效, Deep-sleep 唤醒) |
| I2C_SDA | GPIO19 | MAX30102 数据线 |
| I2C_SCL | GPIO18 | MAX30102 时钟线 |
| DHT11 | GPIO6 | 温湿度传感器 |
| BATT_ADC | GPIO4 | 电池电压检测 (ADC_CH4) |
| PPG_LED | GPIO12 | PPG 采集状态灯 (硬件固定) |
| SYS_LED | GPIO13 | 系统工作状态灯 (硬件固定) |
| SD_CS | GPIO7 | TF 卡片选 |
| SD_CLK | GPIO11 | TF 卡 SPI 时钟 |
| SD_MOSI | GPIO3 | TF 卡 SPI 主出 |
| SD_MISO | GPIO10 | TF 卡 SPI 主入 |
| BOOT | GPIO9 | BOOT 按钮 (低电平=按下) |
| BUTTON1 | GPIO2 | 用户按钮 (单击=切换模式, 双击=WiFi, 长按=BLE) |
| EXT_FLASH | GPIO14-17 | 外挂 Flash (硬件占用) |
| UART0_TX | GPIO20 | 调试串口输出 |
| UART0_RX | GPIO21 | 调试串口输入 |

---

## 目录结构

```
esp32c3/
├── CMakeLists.txt              # 项目根配置
├── partitions.csv              # Flash 分区表 (4MB)
├── sdkconfig.defaults          # SDK 默认配置
├── build.sh                    # 编译脚本 (含内存使用摘要)
├── main/
│   └── main.c                  # 入口、状态机、常驻任务
├── components/
│   ├── ppg_config/             # 全局配置 (引脚, BLE命令, 系统状态)
│   ├── ble_svc/                # BLE GATT 服务 + 调试日志
│   ├── max30102/               # MAX30102 I2C 驱动 (新API)
│   ├── ppg_algo/               # PPG 算法 (定点运算, 无FPU)
│   ├── sd_storage/             # TF 卡存储 (FAT32, 二进制格式)
│   ├── wifi_prov/              # WiFi 配网 (NVS, 自动连接)
│   ├── wifi_transfer/          # HTTP 服务器 (文件下载, OTA上传)
│   ├── battery/                # 电池 ADC 检测 + 电量计算
│   ├── dht11/                  # DHT11 温湿度驱动
│   ├── ota_upgrade/            # OTA 升级 (双分区, SHA-256)
│   ├── power_mgmt/             # 电源管理 (DFS, Deep-sleep)
│   ├── ppg_log/                # 异步日志系统 (环形缓冲, 文件输出)
│   └── compress/               # LZ4 压缩库
└── output/                     # 编译输出 (bootloader, partition-table, app.bin)
```

---

## 系统状态机

```
冷启动 ──> STATE_BLE_PAIRING (默认启动BLE，方便快速连接)
              │
              ├─ BLE 连接成功 ──> STATE_BLE_CONNECTED
              │                      │
              │                 接收命令 (0x10 添加WiFi, 0x01 开始测量等)
              │                      │
              │                      ▼
              │                 STATE_WIFI_STA (自动连接，30秒超时)
              │                      │
              │                      ▼
              │                 STATE_DEEP_SLEEP
              │
              └─ 超时 ──> STATE_DEEP_SLEEP
                              │
                         GPIO5 唤醒 (MAX30102 中断)
                              │
                              ▼
                         STATE_STANDALONE (独立采集)
                              │
                         30秒活跃后进入 Light-sleep 循环
                              │
                         5分钟无中断 ──> STATE_DEEP_SLEEP
```

**WiFi 连接后自动时间同步**：固件通过 `https://ip.ddnspod.com/timestamp` 获取网络时间戳，跳过证书验证。

---

## 按钮功能

### BUTTON1 (GPIO11)

| 操作 | 功能 |
|------|------|
| 单击 | 切换 STANDALONE / MEASURING 模式 |
| 双击 | 进入 WiFi 模式 |
| 长按 3秒 | 进入 BLE 配对模式 |

### BOOT (GPIO9)

| 操作 | 功能 |
|------|------|
| Deep-sleep 倒计时中按下 | 取消 Deep-sleep |
| BLE 配对超时中按下 | 取消配对 |

---

## BLE 通信协议

### GATT 服务

```
Service UUID: 0000fff0-0000-1000-8000-00805f9b34fb
├── 0xFFF1: Status      (Read/Notify) - 20字节 设备状态
├── 0xFFF2: Live Data   (Notify)      - 5字节 PPG数据
├── 0xFFF3: Command     (Read/Write/Notify) - 帧协议命令
└── 0xFFF4: File List   (Read)         - TF卡文件列表 JSON
```

### 帧协议

```
请求帧: [0xAA][CMD][LEN][DATA...][CHECKSUM]
响应帧: [0xAA][CMD][0x01][STATUS][CHECKSUM]
数据帧: [0xAA][CMD][LEN][DATA...][CHECKSUM]

CHECKSUM = SUM(CMD + LEN + DATA 各字节) & 0xFF
```

**状态码**: 0=OK, 1=取消, 2=校验错误, 3=未知命令, 4=电量不足

### 命令表

| CMD | 名称 | 数据 | 响应 | 说明 |
|-----|------|------|------|------|
| 0x01 | START_MEASURE | - | OK | 开始 PPG 采集 |
| 0x02 | STOP_MEASURE | - | OK | 停止 PPG 采集 |
| 0x03 | START_WIFI | - | OK / 0x04 | 启动 WiFi (电量>=20%) |
| 0x10 | WIFI_ADD | SSID_LEN(2B)+SSID+PWD_LEN(2B)+PWD | OK | 添加 WiFi，自动连接 |
| 0x11 | WIFI_STATUS | - | OK | 查询 WiFi 状态 |
| 0x12 | WIFI_CLEAR | - | OK | 清除所有已保存 WiFi |
| 0x13 | WIFI_DELETE | index(1B) | OK | 按索引删除 WiFi |
| 0x14 | WIFI_LIST | - | JSON via 0xFFF4 | 获取已保存 WiFi 列表 |
| 0x20 | OTA_ENTER | - | OK | 进入 OTA 模式 |
| 0x21 | FW_VERSION | - | version via 0xFFF1 Notify | 获取固件版本 |
| 0x22 | QUERY_STATUS | - | OK (更新 0xFFF1) | 刷新状态数据 |
| 0x23 | QUERY_SD_CARD | - | Data(4B) | 获取 SD 卡空间 |
| 0x24 | QUERY_BATTERY | - | Data(1B) | 获取电池电量百分比 |
| 0x30 | LOG_LEVEL | level(1B) | OK | 设置日志级别 |
| 0x31 | LOG_STATUS | - | OK | 查询日志状态 |
| 0x32 | FILE_DOWNLOAD | - | IP string | BLE 触发 WiFi，返回设备 IP，App 通过 HTTP 下载 |
| 0x40 | TIME_SYNC | timestamp(4B) | OK | 同步 Unix 时间戳 (大端序, UTC+8) |
| 0x41 | STANDALONE | - | OK | 进入独立采集模式 |
| 0x50 | UART_RECORD | enable(1)+baud(4B) | OK | 串口记录控制 |

### WiFi 添加帧格式 (0x10)

```
[0x10][SSID_LEN_H][SSID_LEN_L][SSID...][PWD_LEN_H][PWD_LEN_L][PWD...]
```

- SSID_LEN: uint16 大端序
- PWD_LEN: uint16 大端序

**注意**: 添加 WiFi 凭据后，固件自动进入 STATE_WIFI_STA 连接。

### SD 卡查询响应 (0x23)

```
[0xAA][0x23][0x04][FREE_H][FREE_L][TOTAL_H][TOTAL_L][CHECKSUM]
```

- FREE: uint16 大端序，剩余空间 MB
- TOTAL: uint16 大端序，总空间 MB

### 电池查询响应 (0x24)

```
[0xAA][0x24][0x01][BATT_PCT][CHECKSUM]
```

- BATT_PCT: uint8，电池电量百分比 (0-100)

### 串口记录命令 (0x50)

```
[0x50][ENABLE][BAUD_H][BAUD_2][BAUD_1][BAUD_L]
```

- ENABLE: uint8，1=开始记录，0=停止记录
- BAUD: uint32 大端序，波特率（支持 9600~5000000）

**示例：开始记录 115200 波特率**：
```
0x50 0x01 0x00 0x01 0xC2 0x00
```

**数据存储**：
- 目录：`/sdcard/uart0/`
- 文件名：`YYYYMMDD_HHMMSS.log`
- 单文件限制：10MB，超过后自动创建新文件
- 双缓冲区：2 x 32KB，10ms 读取间隔，支持最高 5Mbps

### Status 特征值 (0xFFF1) - 20 字节

```
偏移  长度  字段       类型    说明
0     1     batt_pct   uint8   电池电量百分比 (0-100)
1-3   3     reserved   uint8   保留 (0x00)
4     1     connected  uint8   WiFi 连接状态 (0=未连接, 1=已连接)
5-19  15    version    char[]  固件版本字符串 (UTF-8, 空字符填充)
```

### Live Data 特征值 (0xFFF2) - 5 字节

```
偏移  长度  字段     类型    说明
0-1   2     hr       uint16  心率 (BPM, 大端序)
2     1     spo2     uint8   血氧饱和度 (0-100%)
3     1     pi       uint8   灌注指数 (保留, 0x00)
4     1     quality  uint8   信号质量 (80=有效, 20=无效)
```

---

## BLE 调试

BLE 调试宏默认开启：

```c
// components/ble_svc/ble_svc.c
#define BLE_DEBUG_ENABLE    1  // 0=关闭, 1=开启
```

**日志示例**：
```
[BLE] RX Raw (5 bytes): AA 01 00 00 AB
[BLE] RX Frame: cmd=0x01 data_len=0 total=5
[BLE] Checksum: expected=0xAB actual=0xAB OK
[BLE] RX Enqueued: cmd=0x01
[BLE] TX Response: cmd=0x01 status=0 checksum=0x02
[BLE] TX Raw (5 bytes): AA 01 01 00 02
```

---

## 电池检查

电池低电保护可通过宏开关禁用（调试用）：

```c
// ppg_config.h
#define BATTERY_CHECK_ENABLE    0  // 0=禁用, 1=启用
```

**影响范围**：
- PPG 采集：低电停止采集 (最低 10%)
- WiFi 连接：低电禁止开启 (最低 20%)
- 电源监控：连续 3 次低电压触发关机

### 硬件配置

- **分压电阻**: 100K + 100K，分压比 = 2x
- **ADC 量程**: 0-2.5V (ADC_ATTEN_DB_12)
- **滤波电容**: 100nF X7R 陶瓷电容
- **采样方式**: 64 次均值，结果 ×100 (如 420 = 4.20V)

### 电量查表曲线

| 电压 (×100) | 电量 % |
|-------------|--------|
| 420 | 100 |
| 410 | 90 |
| 400 | 80 |
| 390 | 65 |
| 380 | 50 |
| 370 | 35 |
| 360 | 20 |
| 350 | 10 |
| 340 | 0 |

> 高压锂电 + LTH7R 充电芯片，满电只能到 4.2V（非 4.35V）。

---

## 编译与烧录

### 编译

```bash
./build.sh          # 增量编译
./build.sh clean    # 清理后全量编译
```

### 烧录

```bash
. $HOME/esp/esp-idf-v6.0.1/export.sh
idf.py -p /dev/ttyUSB0 flash monitor
```

### 串口监控

UART0 (GPIO20/21)，1M 波特率，8N1

### 分区表

| 名称 | 类型 | 子类型 | 偏移 | 大小 |
|------|------|--------|------|------|
| nvs | data | nvs | 0x9000 | 0x8000 (32KB) |
| otadata | data | ota | 0x11000 | 0x2000 (8KB) |
| app0 | app | ota_0 | 0x20000 | 0x1E0000 (1.875MB) |
| app1 | app | ota_1 | 0x200000 | 0x1E0000 (1.875MB) |

---

## HTTP API

WiFi 连接后通过局域网 IP 访问。

| 端点 | 方法 | 说明 |
|------|------|------|
| `/api/status` | GET | 设备状态 JSON |
| `/api/files` | GET | TF 卡文件列表 JSON |
| `/api/download?file=xxx` | GET | 下载文件（带 X-File-CRC32 校验头） |
| `/api/ota` | GET | OTA 升级页面 |
| `/api/ota/info` | GET | OTA 分区信息 JSON |
| `/api/ota` | POST | 上传固件执行 OTA |
| `/api/logs` | GET | 日志文件列表 |
| `/api/logs/download?file=xxx` | GET | 下载日志 |
| `/api/shutdown` | POST | 关闭 WiFi |

**OTA 信息响应示例** (`/api/ota/info`)：
```json
{
  "current_version": "1.0.0",
  "build_time": "2026-06-20 23:00:00",
  "current_size": 1289360,
  "partition_label": "ota_0",
  "partition_offset": "0x00020000",
  "partition_size": 1966080,
  "next_partition": "ota_1"
}
```

---

## LED 指示灯

| LED | GPIO | 行为 |
|-----|------|------|
| SYS_LED | GPIO13 | 每 1 秒翻转 (系统运行) |
| PPG_LED | GPIO12 | 闪烁频率随 PPG 数据率变化 (灭=<30Hz, 600ms=<70Hz, 300ms=<100Hz, 150ms=>=100Hz, 80ms) |

---

## 电源优化

| 模式 | CPU 频率 | 说明 |
|------|----------|------|
| Active | 160MHz | BLE/WiFi/采集时全速运行 |
| Light-sleep | 10MHz | 独立采集模式，定时器每 1 秒唤醒 |
| Deep-sleep | - | 仅 GPIO5 唤醒，约 10uA |

**WiFi 优化**：
- 发射功率降至 8.5dBm (34 * 0.25dBm)
- 启用省电模式
- 指数退避重连 (1s→30s)，最多 10 次尝试
- PMF/WPA3 兼容 (`pmf_cfg.capable = true`)
- WiFi 密码不写入串口日志（安全改进）

### 系统功耗状态

| 状态 | 说明 | 功耗 |
|------|------|------|
| DEEP_SLEEP | GPIO5 唤醒，全部关闭 | 约 5uA |
| LIGHT_SLEEP | 独立采集，定时器唤醒 | 约 100uA |
| MEASURING | CPU 160MHz，I2C + ADC 工作 | 约 20-50mA |
| BLE_CONNECTED | 1 秒连接间隔 | 约 474uA |
| WIFI_STA | 连接路由器 + HTTP Server | 约 60-80mA |

### DFS 动态调频

使用 ESP-IDF `esp_pm_configure()` API：max_freq=160MHz，min_freq=10MHz，light_sleep_enable=true。

---

## 数据存储

### 文件目录结构

```
/raw/20260614_22.bin      # 原始 PPG 数据，每小时一个文件
/csv/20260614.csv         # 算法计算结果 (HR/SpO2/PI)
/env/20260614_120000.bin  # DHT11 温湿度数据
/log/20260615_223000.log  # 运行日志，单文件最大 10MB，自动轮转
```

### 二进制格式

所有数据以二进制格式存储，带 XOR 校验：

| 记录类型 | 大小 | 字段 |
|----------|------|------|
| PPG 原始 | 13B | timestamp(4) + red(4) + ir(4) + checksum(1) |
| PPG 结果 | 14B | timestamp(4) + heart_rate(2) + spo2(2) + hr_valid(1) + spo2_valid(1) + reserved(3) + checksum(1) |
| DHT11 | 9B | timestamp(4) + temperature_x10(2) + humidity_x10(2) + checksum(1) |

### SPI 时钟策略

| 阶段 | 时钟 | 说明 |
|------|------|------|
| 初始化 | 400kHz | 卡初始化 |
| 正常操作 | 20MHz | 正常读写 |

---

## 已知问题与修复

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 栈溢出 (vfprintf) | picolibc vfprintf 使用约 8KB 栈 | main.c 中用 puts() 替代 ESP_LOGI |
| MAX30102 未找到 | I2C 通信失败 | 检查硬件连接 |
| Deep-sleep 唤醒失败 | 误用 Light-sleep API | 使用 esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown() |
| BLE 校验码不匹配 | App 用 XOR，固件用 SUM | 已修复：双方统一用 SUM |
| WiFi Add 解析错误 | 固件用 1 字节 SSID_LEN，App 发 2 字节 | 已修复：固件改为 2 字节大端序解析 |

---

## 日志系统

### 架构

```
各任务 -> ppg_log() -> RAM 环形缓冲区 (4KB, mutex 保护)
                           │
                     半满 / 30秒定时 / 关机前 / BLE请求
                           ▼
                      Logger 任务 -> 批量 fwrite -> TF 卡 /log/
```

### 日志格式

```
[unix_ts] [可读时间] [等级] [tag] 函数名:行号 消息
```

示例：`[1750008601] [2026-06-15 22:30:01.234] [E] [ppg_algo] ppg_process:162 SpO2 quality low`

### 日志级别

| 级别 | 值 | 宏 | 说明 |
|------|----|-----|------|
| NONE | 0 | - | 关闭日志 |
| ERROR | 1 | PPG_LOGE | 错误 |
| WARN | 2 | PPG_LOGW | 警告 |
| INFO | 3 | PPG_LOGI | 信息 |
| DEBUG | 4 | PPG_LOGD | 调试 |
| VERBOSE | 5 | PPG_LOGV | 详细 |

- 宏自动传入 `__func__` 和 `__LINE__`
- 运行时通过 BLE 命令 0x30 动态修改

### 文件管理

- 单文件上限 10MB，超过则新建
- 保留最近 5 个未压缩日志文件（约 50MB）
- 超过 5 个的旧文件标记为可归档
- 压缩不在 ESP32 上做（CPU/内存不够），由 App/PC 下载后归档

### 低功耗策略

| 场景 | 行为 |
|------|------|
| Deep-sleep | 环形缓冲区丢弃，不写 TF 卡 |
| 测量中 | 缓冲到 RAM，30 秒或半满刷写一次 |
| 电量 < 10% | 自动降为 ERROR only |
| TF 卡未挂载 | 仅缓冲到 RAM，挂载后批量写入 |

### UART 输出

- UART0 (GPIO20/21)，1M 波特率
- 默认开启 (`PPG_LOG_UART_ENABLE=1`)
- 量产固件可关闭（零开销）

---

## PPG 算法

全定点运算，无 FPU 依赖。

| 算法 | 方法 | 代码大小 |
|------|------|----------|
| 心率 (HR) | 时域峰值检测（滑动窗口 + 自适应阈值） | 约 5KB |
| 血氧 (SpO2) | R/IR 比值，DC/AC 分量，查找表校准 | 约 3KB |
| 灌注指数 (PI) | AC/DC × 100% | 约 1KB |
| 带通滤波器 | IIR 带通 0.5-4Hz | 约 2KB |
| **合计** | | **约 15KB** |

---

## 内存占用估算

| 组件 | RAM |
|------|-----|
| FreeRTOS + 系统 | 20KB |
| NimBLE | 25KB |
| WiFi + lwIP (动态加载) | 35KB |
| HTTP Server (动态加载) | 10KB |
| FAT32 + VFS | 15KB |
| 算法运行时 | 10KB |
| PPG 缓冲 + 应用 | 20KB |
| 日志系统 (环形缓冲 + 任务) | 11KB |
| **峰值 (全功能)** | **约 116KB** |
| **剩余可用** | **约 204KB** |

---

## FreeRTOS 任务设计

**常驻任务**（始终运行）：

| 任务 | 优先级 | 栈大小 | 说明 |
|------|--------|--------|------|
| sys_led_task | 1 | 2KB | GPIO13 系统心跳灯，每 1 秒翻转，喂看门狗 |
| ppg_led_task | 1 | 2KB | GPIO12 PPG 状态灯，闪烁频率随数据率变化 |
| button1_task | 2 | 2KB | BUTTON1 按钮检测（单击/双击/长按） |

**采集任务**（按需创建/销毁）：

| 任务 | 优先级 | 栈大小 | 说明 |
|------|--------|--------|------|
| ppg_task | 5 | 4KB | I2C 读取 MAX30102 + 算法处理 + TF 卡写入 |
| power_task | 1 | 2KB | ADC 电池电压监控，10 秒间隔 |
| dht11_task | 2 | 2KB | DHT11 温湿度采集，10 秒间隔 |

**BLE 任务**（BLE 初始化时创建）：

| 任务 | 优先级 | 栈大小 | 说明 |
|------|--------|--------|------|
| ble_cmd_task | 3 | 4KB | BLE 命令处理（从回调中解耦） |

**看门狗**：Task WDT 5 秒超时，由 `esp_task_wdt_init()` 配置，各任务通过 `esp_task_wdt_reset()` 喂狗。

---

## 开发规范

### 模块解耦

- **组件独立**：每个组件 (max30102, battery, ppg_algo, sd_storage, ble_svc, wifi_prov, wifi_transfer, ppg_log, power_mgmt) 独立编译，通过头文件公开接口，禁止跨组件直接访问内部变量
- **依赖方向**：上层模块依赖下层模块，禁止反向依赖和循环依赖。main.c 作为唯一编排层
- **接口最小化**：每个 `.h` 文件只暴露必要的公共函数，内部实现函数声明为 `static`
- **配置集中管理**：所有引脚、常量、阈值集中在 `ppg_config.h`

### 内存安全

- **malloc/free 配对**：所有动态分配必须检查返回值，错误路径必须释放已分配内存。优先使用栈分配或静态缓冲区
- **缓冲区边界检查**：用 `snprintf` 替代 `sprintf`，`strncpy` 替代 `strcpy`，始终传入缓冲区长度
- **指针非空校验**：公共 API 入口必须校验指针参数，返回 `ESP_ERR_INVALID_ARG`
- **栈溢出防护**：FreeRTOS 任务栈大小留 20% 余量，关键任务启用溢出检测钩子
- **DMA 缓冲对齐**：SPI/I2C DMA 缓冲区使用 `heap_caps_malloc(size, MALLOC_CAP_DMA)`

### 并发安全

- **锁顺序一致**：多把锁场景下，所有任务必须按相同顺序获取锁 (如 spi_mutex -> log_mutex)，避免死锁
- **持锁时间最短**：锁内只做数据拷贝和状态更新，禁止在锁内调用阻塞函数 (I2C/SPI 传输、文件写入)
- **超时获取锁**：优先使用 `xSemaphoreTake(mutex, pdMS_TO_TICKS(timeout))` 而非 `portMAX_DELAY`
- **ISR 安全**：中断服务程序中只使用 `xSemaphoreGiveFromISR` 发信号，禁止调用任何阻塞 API
- **任务间通信**：优先使用队列 (xQueueSend/Receive) 传递数据，避免共享全局变量

### 命名规范

- 私有函数/变量：`s_` 前缀 (如 `s_initialized`)
- 公共 API 函数：组件名开头 (如 `max30102_init`)
- 每个 `.h` 文件顶部有模块功能说明，每个公共函数有 Doxygen 注释

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-06-19 | 初始版本 |
