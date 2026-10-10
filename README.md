# OLED 小猫时钟（STM32F103C8T6 + SSD1306 + EC11）

128x64 的 SSD1306 OLED 上显示一只会眨眼的小猫 + 实时时钟（片内 RTC）。
EC11 旋转编码器翻页、CON/PSH 进入校时、BACK 退出。

## 一、接线方案

模块丝印：`VCC GND SDA SCL(SCK/SDL) TRA TRB PSH CON BACK`

| 模块引脚 | STM32F103C8T6 | 说明 |
| -------- | ------------- | ---- |
| VCC      | 3.3V          | 多数模块 3.3~5V 通用；这里统一接 3.3V，与单片机同电源 |
| GND      | GND           | 必须与单片机共地（不共地一定不亮） |
| SDA      | **PB7**       | 软件 I2C 数据线（可改 `Core/Inc/pins.h`） |
| SCL/SCK  | **PB6**       | 软件 I2C 时钟线 |
| TRA      | **PA0**       | EC11 A 相，接 TIM2_CH1（编码器接口，不可改脚） |
| TRB      | **PA1**       | EC11 B 相，接 TIM2_CH2（编码器接口，不可改脚） |
| PSH      | **PA2**       | EC11 自带按键，内部上拉，按下为低 |
| CON      | **PA3**       | 确认键，内部上拉，按下为低 |
| BACK     | **PA4**       | 返回键，内部上拉，按下为低 |

补充说明：

- EC11 的公共端（C 脚）模块内部一般已接地，所以只引出 TRA/TRB/PSH 三根；
  若你用的是裸编码器，请把 C 脚（三个脚那一侧中间的脚）接到 GND。
- 按键默认"按下 = 低电平"。如果你的模块是高电平有效，改 `Core/Inc/pins.h` 里的
  `KEY_ACTIVE_LEVEL` 为 `1`。
- 编码器转一圈 20 个定位（20 脉冲/圈），代码按 4 倍频处理，转一个"咔哒"= 1 格。

### 下载/调试口（ST-Link V2）

| ST-Link | 单片机 |
| ------- | ------ |
| 3.3V    | 3.3V（若板子自供电可不接） |
| GND     | GND |
| SWDIO   | PA13 |
| SWCLK   | PA14 |

## 二、交互说明

| 操作 | 结果 |
| ---- | ---- |
| 旋转 EC11 | 主界面 ⇄ 大时钟界面 翻页 |
| CON 或 PSH | 进入校时；校时中 = 切到下一项 |
| BACK | 校时中 = 放弃修改退出；主界面 = 回到第一页 |
| 校时中旋转 EC11 | 调整当前反白字段（时/分/秒/年/月/日） |
| 最后一项（日）再按 CON | 保存并写入 RTC |

小猫每 5 秒眨眼一次。

## 三、编译 / 在线烧录 / 调试（VSCode / CodeBuddy）

工具链已装好（gcc 15.2.1 / cmake / ninja / openocd 在 `C:\Users\86139\stm32-tools`）。

| 动作 | 方式 |
| ---- | ---- |
| 编译 | `Ctrl+Shift+B`（Build 任务 = `cmake --workflow --preset build`） |
| 在线烧录 | 终端 → 运行任务 → **Flash (ST-Link)**（自动先编译再 OpenOCD 烧录并复位运行） |
| 在线调试 | `F5`（cortex-debug + OpenOCD，自动停在 `main`） |
| 整片擦除 | 任务 **Erase (ST-Link)** |

命令行等价：

```powershell
cmake --workflow --preset build
openocd -f interface/stlink-hla.cfg -f target/stm32f1x.cfg -c "program build/oled1.elf verify reset exit"
```

> 必须用 `stlink-hla.cfg`：本机 ST-Link V2 固件是 V2J23，只支持旧版 HLA 协议，
> 用 `stlink.cfg` 会报 `only supports deprecated HLA`。

> 不要点右上角的"生成活动文件"，那是单文件编译，必然报找不到 `stm32f1xx.h`。

## 四、时间与掉电保持

- 时钟源优先用 PC14/PC15 上的 32.768kHz 晶振（LSE，精度高）；
  没起振会自动退到内部 LSI（约 40kHz，误差较大，仅兜底）。
- **掉电保持需要把纽扣电池接到 VBAT**：电池正极 → VBAT，负极 → GND。
  没有电池时，每次断电再上电都会回到"固件编译时刻"。
- 时间初值 = **固件编译时刻** + 时区偏移：每次 cmake configure 会把当前时间注入
  `RTC_BUILD_*` 宏，烧录后首次上电自动写进 RTC，**不用手改头文件**。
  改时区改 `Core/Inc/rtc_time.h` 的 `RTC_TZ_OFFSET_SEC`（东八区 = 28800）。
- 手动微调：按 CON 进设置，依次调 时→分→秒→年→月→日，最后再按 CON 保存，BACK 放弃。

## 五、不亮 / 显示异常怎么办

| 现象 | 排查 |
| ---- | ---- |
| PC13 灯快闪（10Hz） | I2C 上没找到屏幕：查 VCC/GND/SDA/SCL 是否接对、是否共地 |
| 全黑但灯慢闪 | 换地址：`Core/Inc/ssd1306.h` 的 `SSD1306_ADDR7` 改成 `0x3D` |
| 花屏/通信不稳 | `Core/Src/i2c_soft.c` 里 `SI2C_HALF_PERIOD_US` 从 2 改大到 5（降速） |
| 左右/上下镜像 | `ssd1306_init()` 里的 `0xA1`（段重映射）与 `0xC8`（COM 扫描方向）取反 |
| 编码器方向反了 | `Core/Src/ec11.c` 里 `EC11_REVERSE` 改成 `1` |
| 时间走不准 | 确认外部 32.768kHz 晶振是否起振（`rtc_uses_lse()` 返回 1 才准） |

## 六、目录结构

```
Core/Inc/pins.h        接线定义（改脚只看这里）
Core/Src/main.c        上电初始化 + 主循环
Core/Src/i2c_soft.c    软件 I2C（PB6/PB7）
Core/Src/ssd1306.c     SSD1306 驱动 + 显存 + 点阵绘制
Core/Src/font5x7.c     5x7 字体
Core/Src/rtc_time.c    片内 RTC + 公历换算
Core/Src/uart_frame.c  USART1 + DMA，收上位机发来的图像帧
Core/Src/ec11.c        TIM2 编码器接口（20 定位/圈）
Core/Src/keys.c        三键消抖
Core/Src/ui.c          四个界面（主页/时钟/相机/校时）与交互
Core/Src/cat_bitmap.c  小猫位图（tools/gen_cat.js 生成，勿手改）
tools/gen_cat.js       位图生成脚本（node tools/gen_cat.js，会打印 ASCII 预览）
tools/cam_serial.py    PC 端：摄像头采集 → 二值化 → 串口发给单片机
```

## 七、串口图像：把 PC 的画面推到 OLED 上

STM32F103 **没有 USB 主机**，不能直接接 UVC 摄像头。做法是让 PC 负责采集和
处理，单片机只负责显示：

```
摄像头 → PC(OpenCV) → 128x64 1bpp(1024 字节) → 串口 → STM32 → OLED
```

### 接线

| USB-TTL 模块 | STM32 | 说明 |
| ---- | ---- | ---- |
| TXD | PA10 | 模块发、单片机收 |
| RXD | PA9 | 单片机发、模块收（调试用，可以不接） |
| GND | GND | **必须共地** |
| 5V/VCC | — | 不要给单片机供 5V（会烧），单片机自己供电 |

模块在设备管理器里看到的 COM 口就是下面命令里的 `--port`。
波特率 921600（改 `Core/Inc/pins.h` 的 `HOST_UART_BAUD`）。

### 帧协议

```
[0]     0xAA
[1]     0x55
[2..3]  payload 长度，小端（固定 1024）
[4]     命令字 0x01 = 1bpp 位图
[5..]   payload 1024 字节
[最后]  校验：从 [2] 到最后一个 payload 字节的累加和 & 0xFF
```

位图格式与 SSD1306 显存一致，收到后直接 memcpy 进显存：
8 页 x 128 列，每字节 `bit0` = 该页最上一行、`bit7` = 最下一行，列顺序 x = 0..127。

### PC 端用法

```powershell
pip install pyserial numpy opencv-python

# 1) 先验证链路（不需要摄像头，发滚动棋盘格）
python tools/cam_serial.py --port COM3 --test

# 2) 接摄像头（先用 --list-cameras 确认编号，别想当然用 0）
python tools/cam_serial.py --port COM3 --camera 1 --mode otsu --fps 20 --preview

# 不知道 COM 几？
python tools/cam_serial.py --list
```

常用参数：`--mode otsu|thresh|dither`（二值化方式）、`--fps`（发送帧率）、
`--mirror`（左右镜像）、`--invert`（黑白反转）、`--preview`（PC 上开窗口对照）。

### 页面切换

- **第一次**收到画面时会自动跳到相机页
- 之后不会被串口帧强行拉回：按 **BACK** 回小猫时钟页，转 **EC11** 在三页间循环
  （小猫时钟 → 大时钟 → 相机）
- 画面在后台持续接收，切回相机页看到的就是最新一帧，**不用关终端**

### 摄像头编号别猜

机器上往往不止一个摄像头，`--camera 0` 通常是笔记本内置那颗：

```powershell
python tools/cam_serial.py --list-cameras
```

USB 摄像头经常是 `1`。**用错编号的表现是：你怎么动摄像头，屏幕都没反应。**
