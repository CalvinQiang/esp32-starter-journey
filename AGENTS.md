# 项目开发规范与 AI 协作准则 (AGENTS.md)

本项目为 **ESP32 物联网实战闯关项目（esp32-starter-journey）**。所有参与本项目的 AI 助理与开发者均需遵循以下协作准则与工程规范。

---

## 📌 一、 核心硬件与环境规范

* **主控芯片**：ESP32-D0WD-V3 / ESP32-WROOM-32E
* **存储配置**：8 MB SPI Flash (`dio` 40MHz) + 2 MB Quad SPI PSRAM
* **开发框架**：ESP-IDF v6.0.2 / FreeRTOS / LVGL v9
* **显示与触摸**：1.69 寸 ST7789 SPI LCD (240×280) + CST816S I2C 电容触摸
* **关键引脚速查**：
  * 板载 LED2：`GPIO27` (高电平点亮)
  * 用户按键 SW3：`GPIO39 (VN)` (输入专用，低电平有效)
  * 屏幕 SPI：CS(`GPIO5`), DC(`GPIO17`), SCLK(`GPIO18`), MOSI(`GPIO19`), RST(`GPIO21`), 背光(`GPIO26`)
  * 触摸 I2C：SCL(`GPIO22`), SDA(`GPIO23`), INT(`GPIO35`)
  * 传感器：DHT11(`GPIO25`), HC-SR04(`GPIO32/33`), SR602(`GPIO34`), NTC(`GPIO36/VP`), WS2812(`GPIO26`)
* **硬件约束**：
  * `GPIO26` 与 LCD 背光复用，调试 WS2812 必须拔下 JP7 跳线帽；
  * `GPIO34/35/36/39` 为纯输入管脚，切勿配置为输出。

---

## 🎯 二、 关卡开发与交付流程规范

项目采用 **“主干演进 + 语义化 Tag 归档”** 模式：

```
[编写当前关卡源码] ───► [本地编译与烧录验证] ───► [学习计划打卡] ───► [Commit + 打 Tag + Push]
```

### 1. 关卡代码组织
* `main/app_main.c` 始终承载**当前关卡**的完整可运行代码。
* 代码必须保持极高可读性，包含：关卡名称、学习目标、核心概念注释与关键参数说明。

### 2. 关卡完成交付标准（三步走）
每次打通一个关卡并验证无误后，依次执行：

1. **更新打卡清单**：
   在根目录 [`ESP32_小白入门实战学习计划.md`](./ESP32_小白入门实战学习计划.md) 中将对应关卡标记为 `[x]`。
2. **提交 Git Commit**：
   采用标准语义化提交信息：
   ```bash
   git add .
   git commit -m "feat(level-<N>): <关卡名称与简短说明>"
   ```
3. **打上版本 Tag 并同步至 GitHub**：
   Tag 命名格式为 `v<N>.0-level-<N>`：
   ```bash
   git tag -a v<N>.0-level-<N> -m "通关：<关卡名称与核心功能>"
   git push origin main --tags
   ```

---

## 💻 三、 编码与构建规范

1. **日志与调试**：
   * 必须使用 `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` 宏代替裸 `printf`（除特殊格式化输出外），并为模块定义清晰的 `TAG`。
2. **延时与任务调度**：
   * 必须使用 FreeRTOS 标准延时 `vTaskDelay(pdMS_TO_TICKS(ms))`，禁止使用空循环死等。
3. **内存管理**：
   * 大块显存、图像 Buffer 或 LVGL 绘制缓存优先使用 `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` 分配至 2MB PSRAM 中。
4. **编译构建命令**：
   * 编译：`idf.py build`
   * 烧录：`idf.py -p <PORT> flash`
   * 串口监视：`idf.py -p <PORT> monitor`
