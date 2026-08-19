# 第 02 章：掌控电流的力量 —— ESP32 GPIO 数字输出与 PWM 呼吸灯

> **写在前面**：在第一章中，我们让单片机向电脑屏幕打印出了文字，实现了“虚拟世界”的数据交互。
> 
> 从这一章开始，我们将正式跨入**“物理世界”**—— 让代码控制芯片管脚释放真实的电流，去点亮开发板上的第一颗硬件蓝色 LED 灯，并进一步利用 PWM（脉冲宽度调制）技术，让灯光如呼吸般优雅地渐亮渐暗！

---

## 2.1 什么是 GPIO？（单片机的手和脚）

**GPIO** 全称是 *General Purpose Input / Output*（**通用输入输出引脚**）。

如果把 ESP32 芯片比作一个人的大脑，那么 GPIO 引脚就是它的**“手”和“脚”**：
* **作为输出（Output，本章内容）**：大脑命令引脚输出 **3.3V 高电平（相当于打开开关供电）** 或 **0V 低电平（相当于关闭开关断电）**，去控制 LED 点亮、蜂鸣器发声、电机旋转；
* **作为输入（Input，下一章内容）**：引脚用来感知外界电平，检测按键有没有被按下、红外传感器有没有探测到人。

```mermaid
flowchart LR
    subgraph ESP32Core ["ESP32 内部控制"]
        Code["代码 gpio_set_level(LED_PIN, 1)"] --> Switch["内部电子开关 (接通 3.3V)"]
    end

    subgraph Hardware ["开发板硬件物理链路"]
        Switch --> Pin27["GPIO27 管脚 (输出 3.3V)"]
        Pin27 --> Resistor["1 kΩ 限流电阻\n(防止电流过大烧坏LED)"]
        Resistor --> BlueLED["板载蓝色 LED2"]
        BlueLED --> GND["GND (0V 地线)"]
    end
```

### 💡 为什么高电平灯会亮？（物理电路原理解密）
查看开发板原理图可以发现：
* **LED2 的正极** 连着 **`GPIO27`**（中间串了一个 1kΩ 的限流电阻）；
* **LED2 的负极** 连着 **`GND（0V 地线）`**；
* 当代码让 GPIO27 输出 **高电平（3.3V）** 时，正负极之间产生 3.3V 电压差，电流流过灯珠，**LED2 亮起**；
* 当代码让 GPIO27 输出 **低电平（0V）** 时，两端没有电压差，电流为零，**LED2 熄灭**。

---

## 2.2 关卡源码逐行带读（两种控制模式）

打开 [`main/app_main.c`](../main/app_main.c)，我们在代码中设计了两个循序渐进的阶段：

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"     // 引入普通 GPIO 控制库
#include "driver/ledc.h"     // 引入硬件 PWM 控制库
#include "esp_log.h"

static const char *TAG = "LEVEL_2_LED";

// 板载蓝色 LED2 物理引脚定义
#define LED_PIN GPIO_NUM_27
```
👉 **第 1 部分：引入引脚与 PWM 控制头文件**
* 想控制引脚的高低电平？借 `driver/gpio.h`；
* 想做呼吸灯渐变？借 `driver/ledc.h`。

---

### 【模式 1】：普通数字输出模式（基础闪烁 Blink）

```c
static void init_led_gpio(void)
{
    // 1. 将引脚恢复为默认干净状态
    gpio_reset_pin(LED_PIN);
    // 2. 把 GPIO27 配置为输出模式 (Output)
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    // 3. 初始输出 0（低电平，熄灭）
    gpio_set_level(LED_PIN, 0);
}
```
* **开机执行 6 次交替闪烁**：
  ```c
  for (int i = 1; i <= 6; i++) {
      gpio_set_level(LED_PIN, 1);       // 输出高电平：开灯！
      vTaskDelay(pdMS_TO_TICKS(500));   // 保持亮 500 毫秒

      gpio_set_level(LED_PIN, 0);       // 输出低电平：关灯！
      vTaskDelay(pdMS_TO_TICKS(500));   // 保持灭 500 毫秒
  }
  ```

---

### 【模式 2】：硬件 PWM 呼吸灯模式（渐亮渐暗）

#### 💡 什么是 PWM？（欺骗人眼的视觉暂留）
单片机的普通 GPIO 只有“纯开（3.3V）”和“纯关（0V）”，**并不能直接输出 1.5V 这样的中间电压**。那怎么让灯呈现 50% 的半亮状态呢？

答案是：**以极快的速度疯狂开关灯（比如每秒开合 5000 次）！**

```text
10% 亮度 (微弱微光)： 
高电平 ──┐          ┌──          ┌──
低电平   └──────────┘  └──────────┘  └──────────  (开的时间短，灭的时间长)

50% 亮度 (适中亮度)： 
高电平 ──────┐      ──────┐      ──────┐
低电平       └──────┘     └──────┘     └──────    (开一半时间，灭一半时间)

90% 亮度 (极亮状态)： 
高电平 ────────────┐ ────────────┐ ────────────┐
低电平             └┘            └┘            └┘ (开的时间极长，几乎全亮)
```

* **占空比（Duty Cycle）**：在一个开关周期内，“开灯时间”占“总周期”的百分比。
* **人眼视觉暂留**：因为一秒钟开关 5000 次太快了，人眼根本感觉不到闪烁，只能感觉到**灯光的平均亮度在连续平滑变化**！

```c
// 呼吸灯核心循环逻辑：
while (1) {
    // 1. 吸气：从 0% 亮度平滑调高到 100%
    for (int duty = 0; duty <= 8191; duty += 150) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(20)); // 每 20ms 微微调亮一点
    }

    // 2. 呼气：从 100% 亮度平滑调低到 0%
    for (int duty = 8191; duty >= 0; duty -= 150) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(20)); // 每 20ms 微微调暗一点
    }
}
```

---

## 2.3 📚 专属编程知识拓展（库函数字典与语法速查）

---

### 1. 本章引入的头文件与 CMake 依赖

| 头文件 | 所属组件 | 对应 CMake REQUIRES | 主要提供的函数 |
| :--- | :--- | :--- | :--- |
| **`"driver/gpio.h"`** | GPIO 基础驱动 | `esp_driver_gpio` 或 `driver` | `gpio_reset_pin()`、`gpio_set_direction()`、`gpio_set_level()` |
| **`"driver/ledc.h"`** | LEDC (PWM) 控制器 | `esp_driver_ledc` | `ledc_timer_config()`、`ledc_channel_config()`、`ledc_set_duty()` |

> ⚠️ **编译避坑提示**：
> 在 ESP-IDF v6 中，如果使用了 `driver/ledc.h`，必须在 [`main/CMakeLists.txt`](../main/CMakeLists.txt) 中的 `REQUIRES` 后面添加 **`esp_driver_ledc`**，否则编译器会提示找不到该头文件。

---

### 2. 核心库函数功能字典

#### ① 普通 GPIO 控制函数三部曲
1. **`gpio_reset_pin(gpio_num_t gpio_num)`**
   * **作用**：将引脚重置回初始默认状态（关闭可能残留的内部上拉/下拉电阻与外设复用绑定）。
2. **`gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode)`**
   * **作用**：设置引脚方向。
   * **常用模式**：`GPIO_MODE_OUTPUT`（输出模式）、`GPIO_MODE_INPUT`（输入模式）。
3. **`gpio_set_level(gpio_num_t gpio_num, uint32_t level)`**
   * **作用**：控制引脚输出电平。
   * **参数**：`1` 输出 3.3V 高电平（亮灯），`0` 输出 0V 低电平（灭灯）。

---

#### ② LEDC (PWM) 硬件控制器配置三部曲
1. **`ledc_timer_config(&ledc_timer)`**
   * **作用**：配置 PWM 定时器的频率和分辨率。
   * **`freq_hz = 5000`**：设置开关频率为 5000 Hz（5 kHz），远超人眼闪烁感知阈值（通常 > 60 Hz 就感觉不到闪烁）；
   * **`duty_resolution = LEDC_TIMER_13_BIT`**：设置 13 位亮度分辨率。也就是把亮度细腻地切分为 $2^{13} = 8192$ 个等级（`0` 代表全灭，`8191` 代表最亮）。
2. **`ledc_channel_config(&ledc_channel)`**
   * **作用**：选择一个硬件通道（如 `LEDC_CHANNEL_0`），并将其与物理引脚（`GPIO27`）进行硬件连通绑定。
3. **`ledc_set_duty()` 与 `ledc_update_duty()`**
   * **`ledc_set_duty(mode, channel, duty)`**：把新的亮度数值写入准备寄存器；
   * **`ledc_update_duty(mode, channel)`**：正式让硬件生效新的占空比（硬件平滑生效，不卡顿 CPU）。

---

## 2.4 动手小实验（即时体验）

烧录当前代码到开发板后，仔细观察板子右下方的蓝色 LED2：
1. **现象确认**：开机前 3 秒，蓝色 LED2 会以 0.5 秒一次的节奏清晰闪烁 6 次；随后自动切换为深呼吸般平滑变亮、变暗的呼吸灯！

### 🧪 实验 1：制作“急促喘息”的呼吸灯
将代码中的亮度变化延时从 `20ms` 改为 `5ms`：
```c
vTaskDelay(pdMS_TO_TICKS(5)); // 从 20 改为 5
```
👉 **重新烧录观察**：呼吸速度是不是瞬间快了 4 倍，像跑完步心跳加速一样？

---

### 🧪 实验 2：制作“微弱夜灯”
将最大亮度限制在 10%（约 800）：
```c
const int max_duty = 800; // 不再升到 8191
```
👉 **重新烧录观察**：LED 会在微弱柔和的光线范围内呼吸，不会刺眼。

---

## 2.5 新手排错宝典

| 遇到的现象 | 常见原因 | 解决方案 |
| :--- | :--- | :--- |
| **烧录成功后，板子上的 LED2 毫无反应** | 1. 看错了灯（板上有两颗灯：LED1 是红色的电源灯，LED2 才是蓝色的受控灯）；<br>2. 引脚宏定义写错了。 | 确认板载引脚定义为 **`GPIO_NUM_27`**，观察蓝色 LED2 灯珠。 |
| **编译时报错 `driver/ledc.h: No such file or directory`** | `main/CMakeLists.txt` 中未声明依赖。 | 打开 `main/CMakeLists.txt`，在 `REQUIRES` 后追加 **`esp_driver_ledc`**。 |

---

## 2.6 本章总结与闯关打卡

恭喜你！学完本章，你已经成功跨入了单片机控制硬件外设的大门：
1. 理解了 GPIO 的本质是控制电压高低（3.3V / 0V）；
2. 掌握了普通数字输出 `gpio_set_level()` 的闪烁控制；
3. 掌握了 PWM 脉宽调制的原理，并用硬件定时器实现了优雅的呼吸灯效果。

---

*下一关预告：既然单片机已经能通过 GPIO 向外“输出”信号了，那么下一关，我们将学习**让单片机“感知输入”—— 读取板载按键 SW3 (GPIO39) 和 SR602 人体红外探头（GPIO34）**，实现“按下按键开关灯”与“人来灯亮、人走灯灭”的人机交互！*
