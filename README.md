# ESP32 Starter Journey 🚀

![ESP32 物联网实战闯关之路](./docs/images/esp32_levels_poster.jpg)

> 基于 **ESP-IDF v6.0** 与 **ESP32-WROOM-32E (8MB Flash + 2MB PSRAM)** 的嵌入式物联网从零进阶实战项目。  
> 集成 1.69 英寸 ST7789 触摸彩屏、LVGL v9 GUI、常用传感器驱动及完整原理图资料。

---

## 🛠️ 硬件参数与特性

| 模块 | 核心规格 | 说明 |
| :--- | :--- | :--- |
| **主控芯片** | **ESP32-D0WD-V3 / ESP32-WROOM-32E** | 双核 240MHz Xtensa LX6，算力 600 DMIPS |
| **存储配置** | **8 MB SPI Flash + 2 MB Quad PSRAM** | 充足的片外内存，轻松运行复杂 LVGL 图形界面 |
| **彩色屏幕** | **1.69 英寸 ST7789 SPI LCD** | 分辨率 240 × 280，色彩绚丽 |
| **电容触摸** | **CST816S (I2C 总线)** | 支持单点触摸、滑动与手势识别 |
| **板载传感器** | **DHT11、HC-SR04、SR602、NTC、WS2812** | 温湿度、超声波测距、人体红外、热敏测温、RGB灯珠 |
| **外部存储** | **MicroSD / TF 卡槽** | 4-bit 高速 SDIO 接口 |
| **下载调试** | **Type-C + CH340C** | 支持 DTR/RTS 硬件自动复位下载 |

---

## 🗺️ 实战通关路线图 (10 关进阶)

本项目配有详细的实战学习任务表，详见 [**`ESP32_小白入门实战学习计划.md`**](./ESP32_小白入门实战学习计划.md)：

- [x] **第 01 关：ESP32 串口通信与 Hello World 打印** 📖 [【阅读深度教程】](./book/01_串口通信与HelloWorld深度解析.md)
- [x] **第 02 关：ESP32 GPIO 数字输出与 PWM 呼吸灯** 📖 [【阅读深度教程】](./book/02_GPIO输出与PWM呼吸灯.md)
- [ ] **第 03 关：ESP32 按键输入检测与人体红外感应**
- [ ] **第 04 关：ESP32 模拟量采集(ADC)、DHT11 温湿度与超声波测距**
- [ ] **第 05 关：ESP32 驱动 1.69寸 ST7789 彩屏与几何图形渲染**
- [ ] **第 06 关：ESP32 搭载 LVGL v9 现代图形界面与 CST816S 电容触摸**
- [ ] **第 07 关：ESP32 Wi-Fi 联网与 SNTP 毫秒级网络授时时钟**
- [ ] **第 08 关：ESP32 HTTP 客户端与实时天气 API 解析(cJSON)**
- [ ] **第 09 关：ESP32 挂载 MicroSD/TF 卡(SDIO)与 FATFS 电子相册**
- [ ] **第 10 关：ESP32 综合实战 —— 桌面多功能智能气象站与中控台**

---

## 📖 开源电子书目录 (`book/`)

本项目已同步建设出版级开源实战教程：👉 [**`book/SUMMARY.md`**](./book/SUMMARY.md)

* 📘 [**第 01 章：ESP32 串口通信与 Hello World 深度剖析**](./book/01_串口通信与HelloWorld深度解析.md)
* 📘 [**第 02 章：ESP32 GPIO 数字输出与 PWM 呼吸灯**](./book/02_GPIO输出与PWM呼吸灯.md)
* 📘 *(后续章节随着实战关卡持续更新)*

---

## 📂 项目目录结构

```text
├── ESP32_小白入门实战学习计划.md   # 10 关实战任务与打卡指南
├── README.md                      # 项目说明文档
├── CMakeLists.txt                 # ESP-IDF 根项目构建脚本
├── sdkconfig.defaults             # 默认芯片配置 (PSRAM / 主频 / LVGL)
│
├── main/                          # 核心业务源码
│   ├── app_main.c                 # 当前关卡源码
│   ├── CMakeLists.txt             # 组件构建配置
│   └── idf_component.yml          # LVGL 与触摸组件依赖
│
├── book/                          # 📖 开源电子书教程（章节解析与知识点精讲）
│   ├── SUMMARY.md                 # 教程目录大纲
│   └── 01_串口通信与HelloWorld深度解析.md
│
├── docs/                          # 硬件原理图与设计资料
│   ├── ESP32开发板原理图1.1.pdf
│   ├── ESP32物联网开发套件使用说明.pdf
│   └── ESP32开发板资料整理_复核修订版.md
│
└── archive/                       # 历史实验源码与归档
```

---

## ⚡ 快速上手与编译烧录

### 1. 编译固件
```bash
idf.py build
```

### 2. 烧录到开发板
```bash
idf.py -p COMx flash
```
*(将 `COMx` 替换为您开发板对应的串口号，如 `COM3`)*

### 3. 打开串口监视器
```bash
idf.py -p COMx monitor
```

---

## 📄 开源许可
MIT License © [CalvinQiang](https://github.com/CalvinQiang)
