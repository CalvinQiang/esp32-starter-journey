# ESP32 Starter Journey 🚀

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

- [x] **关卡 1：串口通信与 Hello World 打印**（运行环境与芯片参数读取）
- [ ] **关卡 2：板载蓝色 LED2 闪烁与呼吸灯控制**（GPIO 输出与 PWM）
- [ ] **关卡 3：按键检测与人体红外感应**（数字输入与软件消抖）
- [ ] **关卡 4：ADC 模拟测温、超声波测距与温湿度采集**（模拟量转换与时序测量）
- [ ] **关卡 5：ST7789 屏幕驱动与几何绘图**（SPI 总线驱动）
- [ ] **关卡 6：LVGL v9 现代图形界面与电容触摸交互**（现代 UI 与 PSRAM 显存）
- [ ] **关卡 7：Wi-Fi 联网与 SNTP 毫秒级网络授时**（网络时钟）
- [ ] **关卡 8：HTTP 实时天气 API 请求与 JSON 数据解析**（物联网数据获取）
- [ ] **关卡 9：MicroSD / TF 卡 FATFS 文件系统挂载与图片相册**（SDIO 存储）
- [ ] **关卡 10：综合大实战 —— 【桌面多功能智能气象站】**（综合作品）

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
