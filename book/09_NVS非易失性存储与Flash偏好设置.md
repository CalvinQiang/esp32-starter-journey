# 第 09 关：ESP32 NVS 非易失性存储与 Flash 偏好设置

![第09关封面插画](../docs/images/esp32_level9_cover.jpg)

---

## 🎯 本关学习目标

在前 8 关的学习中，我们写的所有程序都有一个共同的特点：**一旦拔掉 USB 供电线或者按下复位按键，单片机里的所有变量都会瞬间“清零失忆”！**

但在真实的智能家居产品中：
* 你的 Wi-Fi 路由器账号密码、家里的智能台灯亮度、闹钟时间……**绝对不能一断电就全部丢失**；
* 每次给单片机重新插上电，它必须能够**“回忆”**起断电前保存的所有配置！

本关我们将学习 ESP32 的核心杀手级技术 —— **NVS（Non-Volatile Storage，非易失性键值对存储）**。

完成本关卡后，你将达成以下核心成就：
1. **彻底搞懂 Flash 与 RAM 的区别**：理解为什么普通变量断电会丢，而 Flash 可以保存 10 年不丢；
2. **掌握 NVS 的“抽屉与便利贴”模型**：搞懂命名空间（Namespace）与键值对（Key-Value）结构；
3. **掌握 ESP-IDF NVS 驱动六步闭环**：初始化 ➔ 打开抽屉 ➔ 写入数据 ➔ **`nvs_commit` 关键提交** ➔ 读取数据 ➔ 关闭抽屉；
4. **开发开机计数器与 Wi-Fi 配置管理器**：学会存储整型、字符串和自定义结构体；
5. **实现硬件“恢复出厂设置（Factory Reset）”**：长按 SW3 按键 3 秒，全量擦除 Flash 并重启！

---

## 9.1 为什么单片机断电会“失忆”？RAM 与 Flash 的生动比喻

很多初学单片机的小白常问：**“我在代码里写了 `int my_score = 100;`，拔掉电源再插上，为什么它不能保留 100？”**

```text
 ┌─────────────────────────────────────────────────────────────┐
 │                【RAM 草稿纸 VS Flash 记事本】                │
 │                                                             │
 │  1. SRAM (内部运行内存 520KB / PSRAM 2MB) ➔ 【草稿纸】        │
 │     - 特点: 速度极快(微秒)，CPU 在上面计算变量               │
 │     - 致命点: 【必须通电】！一旦断电，草稿纸上的字瞬间消失！  │
 │                                                             │
 │  2. SPI Flash (板载外部存储 8MB)         ➔ 【硬皮记事本】    │
 │     - 特点: 像光盘和U盘一样，用微观电子陷阱记录数据          │
 │     - 优势: 【断电永不丢失】！即使拔掉电源放 10 年依然完好！ │
 └─────────────────────────────────────────────────────────────┘
```

👉 **结论**：
* 每次我们在代码里声明的局部变量、全局变量，都存放在 **RAM（草稿纸）** 里；
* 如果想让数据断电不丢，我们就必须**把数据“写进 Flash（硬皮记事本）”** 中！

---

## 9.2 什么是 NVS？ESP32 内置的“字典保险箱”

很多单片机（如传统的 51 或部分 STM32）如果想在 Flash 里存数据，程序员必须自己去算扇区地址、自己按字节擦除，一不小心就会把自己的程序固件给擦掉！

**ESP-IDF 的 NVS（Non-Volatile Storage）直接帮我们解决了这个痛点！**
* 它在 8MB Flash 中专门划分了一块安全区域（默认通常为 24KB）；
* 它把这块区域包装成了一个现代化的 **Key-Value（键值对）数据库**（就像 Python 的字典或 JSON 对象一样简单）：
  * `键 (Key)`：数据的名字（如 `"wifi_ssid"`、`"brightness"`）；
  * `值 (Value)`：真实的数据（如 `"MyHomeWiFi"`、`85`）。
* 你只需要告诉它：`存入 key="brightness", value=85`，它就会自动在 Flash 中寻找空闲空间安全存好，并自带**磨损均衡（Wear Leveling）**，保护 Flash 不被反复擦写损坏！

---

## 9.3 命名空间（Namespace）：防止“撞名”的分类抽屉

想象一下，如果你的项目里既有 `wifi` 模块，又有 `display` 模块，两个模块都想保存一个叫 `"status"` 的变量，岂不是打架冲突了？

NVS 引入了 **命名空间（Namespace）** 概念：
* 可以把命名空间想象成**带有标签的大抽屉**；
* 抽屉 A 叫 `"wifi_cfg"`，里面可以放一个叫 `"status"` 的便签；
* 抽屉 B 叫 `"screen_cfg"`，里面也可以放一个叫 `"status"` 的便签；
* 互不干扰，井井有条！

```text
  NVS 存储分区 (8MB Flash)
    ├── 【抽屉 1: "wifi_cfg"】
    │     ├── "ssid"  ➔ "My_Home_5G"
    │     └── "pass"  ➔ "12345678"
    │
    └── 【抽屉 2: "screen_cfg"】
          ├── "brightness" ➔ 80
          └── "dark_mode"  ➔ 1
```

---

## 9.4 📚 核心库函数功能字典与关键参数解密（小白必读）

在看实战代码前，我们先把 NVS 的 **标准 6 步流水线** 与库函数搞清楚：

```mermaid
flowchart TD
    Step1["① 初始化 NVS 分区\nnvs_flash_init()"] --> Step2["② 打开命名空间抽屉\nnvs_open()"]
    Step2 --> Step3["③ 写入或读取数据\nnvs_set_xxx() / nvs_get_xxx()"]
    Step3 --> Step4["④ 【最关键】提交保存到 Flash\nnvs_commit()"]
    Step4 --> Step5["⑤ 关闭抽屉释放句柄\nnvs_close()"]
```

---

### 1. 🛠️ 本章引入的全新头文件与 CMake 依赖

| 头文件 | 作用说明 | 对应 CMake REQUIRES | 核心函数 / 宏 |
| :--- | :--- | :--- | :--- |
| **`"nvs_flash.h"`** | **NVS 分区底层初始化与全盘格式化** | **`nvs_flash`** | `nvs_flash_init()`、`nvs_flash_erase()` |
| **`"nvs.h"`** | **NVS 键值对增删改查操作** | `nvs_flash` | `nvs_open()`、`nvs_set_xxx()`、`nvs_get_xxx()`、`nvs_commit()` |

---

### 2. 🎛️ 核心函数与关键细节深度解密

#### ① `nvs_flash_init()` 与 分区容错处理
* **生活比喻**：检查 Flash 上的记事本格式是否完好；
* **标准写法**：
  ```c
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      // 如果 Flash 满了或者分区表更新导致格式不兼容，先全盘擦除再初始化
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  ```

#### ② `nvs_open(namespace, open_mode, &handle)`
* **生活比喻**：拉开一个指定的分类抽屉；
* **参数 `open_mode`**：
  * `NVS_READONLY`：只读模式（省电安全）；
  * `NVS_READWRITE`：可读可写模式（最常用）。
* **返回 `handle`**：抽屉的操作把手（句柄）。

#### ③ `nvs_set_xxx()` 与 `nvs_get_xxx()`
* **支持丰富的数据类型**：
  * 32位整型：`nvs_set_i32(handle, "boot_count", 42)` / `nvs_get_i32(handle, "boot_count", &val)`；
  * 字符串：`nvs_set_str(handle, "ssid", "MyWiFi")` / `nvs_get_str(handle, "ssid", buf, &len)`；
  * 结构体/二进制块：`nvs_set_blob(handle, "custom_struct", &data, sizeof(data))`。

#### ④ 🚨 为什么必须调用 `nvs_commit(handle)`？（小白最常踩的坑 ⚠️）
* **大白话**：当你调用 `nvs_set_i32()` 时，数据其实只是暂时写在**内存缓冲区（剪贴板）**里，**并没有真正通电写入物理 Flash 颗粒**！
* **`nvs_commit(handle)` 就像你在 Word 里按下 `Ctrl + S` 保存键**！如果不写这行代码，断电后数据依然会丢失！

---

## 9.5 实战第 1 步：开机启动计数器（断电不丢失）

我们先来做最经典、最直观的实验：让 ESP32 记录自己一生中被开机了多少次！

> 📁 **配套源码文件**：[`code/09_nvs_storage/01_boot_counter.c`](../code/09_nvs_storage/01_boot_counter.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 9 1 --flash` 即可秒级切换并自动烧录！

### 🌟 实验 1 完整源码：

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "EXP1_BOOT_COUNTER";

#define NVS_NAMESPACE   "app_data"
#define KEY_BOOT_COUNT  "boot_count"

void app_main(void)
{
    // 1. 初始化 NVS 底层分区
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2. 打开命名空间抽屉
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));

    // 3. 读取历史开机次数 (带容错处理)
    int32_t boot_count = 0;
    err = nvs_get_i32(nvs_handle, KEY_BOOT_COUNT, &boot_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "💡 未找到历史记录！这是首次烧录后的【第 1 次开机】！");
        boot_count = 0;
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "📖 从 Flash 读到历史开机记录: \033[32m%ld\033[0m 次", (long)boot_count);
    }

    // 4. 次数 +1 并写回
    boot_count++;
    ESP_LOGI(TAG, "✍️ 正在写回最新开机次数: \033[36m%ld\033[0m 次...", (long)boot_count);
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, KEY_BOOT_COUNT, boot_count));

    // 5. 关键提交 (Ctrl + S)
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));

    // 6. 关闭抽屉
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "🎉 数据已固化！按一下板子上的 RST 物理按键试试看！");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

> [!TIP]
> **💡 动手试试看**：
> 烧录完成后，打开串口监视器，按下开发板上的 **`EN / RST` 复位按键**（或直接拔掉 USB 线重新插上）。你会惊喜地发现：计数值从 1 变成 2、3、4……断电绝不丢失！

---

## 9.6 实战第 2 步：用户偏好设置与 Wi-Fi 账号密码持久化

学会存数字后，我们来模拟一个真实的智能家居设备，把 **Wi-Fi 账号、密码、屏幕亮度与深色模式开关** 一起存入 Flash！

> 📁 **配套源码文件**：[`code/09_nvs_storage/02_preferences_rw.c`](../code/09_nvs_storage/02_preferences_rw.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 9 2 --flash` 即可秒级切换并自动烧录！

### 🌟 实验 2 核心代码解析：

```c
// 1. 存入字符串与整型
ESP_ERROR_CHECK(nvs_set_str(handle, "wifi_ssid", "My_Home_WiFi_5G"));
ESP_ERROR_CHECK(nvs_set_str(handle, "wifi_pass", "SuperSecret123"));
ESP_ERROR_CHECK(nvs_set_i32(handle, "brightness", 85));
ESP_ERROR_CHECK(nvs_set_i32(handle, "dark_mode", 1));
ESP_ERROR_CHECK(nvs_commit(handle)); // 提交保存

// 2. 读取字符串 (两步安全法：先查长度，再分配读取)
size_t required_size = 0;
char ssid[64] = {0};
if (nvs_get_str(handle, "wifi_ssid", NULL, &required_size) == ESP_OK) {
    nvs_get_str(handle, "wifi_ssid", ssid, &required_size);
    ESP_LOGI(TAG, "📶 成功读取 Wi-Fi 名称: %s", ssid);
}
```

---

## 9.7 实战第 3 步：综合大工程 —— 配置管理中心与按键长按恢复出厂设置

真正的消费级电子产品（如智能音箱、路由器）都具备一个 **“长按 Reset 恢复出厂设置”** 的物理功能。以下是完整的工程代码：

> 📁 **配套源码文件**：[`code/09_nvs_storage/03_nvs_factory_reset.c`](../code/09_nvs_storage/03_nvs_factory_reset.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 9 3 --flash` 即可秒级切换并自动烧录！

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "LEVEL09_MANAGER";

#define SW3_BUTTON_PIN      GPIO_NUM_39
#define LED2_PIN            GPIO_NUM_27
#define NVS_NAMESPACE       "device_cfg"
#define KEY_RUN_SECONDS     "run_seconds"

/* 恢复出厂设置：清空 NVS 并重启芯片 */
static void perform_factory_reset(void)
{
    ESP_LOGW(TAG, "🚨 触发长按 3 秒！正在执行【恢复出厂设置】...");
    
    // LED2 快闪 5 次警示
    for (int i = 0; i < 5; i++) {
        gpio_set_level(LED2_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(LED2_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 格式化擦除整个 NVS 分区
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_LOGI(TAG, "🧹 NVS 分区已清空！系统将在 1 秒后自动重启...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

/* 按键长按监控任务 */
static void task_button_monitor(void *pvParameters)
{
    int press_counter = 0;
    while (1) {
        if (gpio_get_level(SW3_BUTTON_PIN) == 0) { // SW3 按下
            press_counter++;
            gpio_set_level(LED2_PIN, 1);
            ESP_LOGW(TAG, "⚠️ 正在长按 SW3 按键重置倒计时: %d / 3 秒...", press_counter);
            if (press_counter >= 3) {
                perform_factory_reset();
            }
        } else {
            press_counter = 0;
            gpio_set_level(LED2_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // 初始化硬件与 NVS
    // ...
    xTaskCreate(task_button_monitor, "btn_task", 2048, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        // 每 5 秒自动将累计运行时间刷入 Flash
    }
}
```

---

## 9.8 关卡总结与通关打卡

太棒了！你已经攻克了单片机存储领域的核心山头 —— **Flash 非易失性存储**！

### 🏆 核心技能清单回顾：
* [x] **存储本质**：理解 RAM（通电草稿纸）与 Flash（永久笔记本）的物理差异；
* [x] **NVS 机制**：掌握命名空间抽屉与 Key-Value 键值对读写模型；
* [x] **避坑铁律**：牢记必须调用 `nvs_commit()` 才能真正落盘保存；
* [x] **出厂重置**：掌握 `nvs_flash_erase()` 与 `esp_restart()` 恢复出厂设置。

---

接下来，我们将推开**视觉显示的大门**！请翻开 [**第 10 章：ESP32 驱动 1.69寸 ST7789 彩屏与几何图形渲染**](./10_ST7789彩屏驱动与几何图形渲染.md)，让我们用高速 SPI DMA 把绚丽的色彩画在开发板的液晶屏上！
