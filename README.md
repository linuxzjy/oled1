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
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/oled1.elf verify reset exit"
```

> 不要点右上角的"生成活动文件"，那是单文件编译，必然报找不到 `stm32f1xx.h`。

## 四、时间与掉电保持

- 时钟源优先用 PC14/PC15 上的 32.768kHz 晶振（LSE，精度高）；
  没起振会自动退到内部 LSI（约 40kHz，误差较大，仅兜底）。
- **掉电保持需要把纽扣电池接到 VBAT**：电池正极 → VBAT，负极 → GND。
  没有电池时，每次断电再上电都会回到 `Core/Inc/rtc_time.h` 里的默认时间。
- 首次烧录前把 `rtc_time.h` 的 `DEFAULT_YEAR/MON/DAY/HOUR/MIN/SEC` 改成当前时间，
  上电后就是对的；之后也可以直接用按键校时。

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
Core/Src/ec11.c        TIM2 编码器接口（20 定位/圈）
Core/Src/keys.c        三键消抖
Core/Src/ui.c          三个界面与交互
Core/Src/cat_bitmap.c  小猫位图（tools/gen_cat.js 生成，勿手改）
tools/gen_cat.js       位图生成脚本（node tools/gen_cat.js，会打印 ASCII 预览）
```
