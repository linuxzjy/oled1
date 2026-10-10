# AGENTS.md — 本工程上下文（AI 助手会自动读取）

STM32F103C8T6 裸机工程：CMSIS 寄存器开发（无 HAL/无 SPL），
CMake + Ninja + arm-none-eabi-gcc 构建，OpenOCD + ST-Link 在线烧录/调试。
功能：SSD1306(128x64, I2C) 显示小猫 + RTC 实时时钟，EC11 编码器交互。

## 本机环境（换电脑需更新此段路径）

- 工具链根目录：`C:\Users\86139\stm32-tools\`（gcc / cmake / ninja / openocd）
- 版本：arm-none-eabi-gcc 15.2.1、cmake 4.2.0、ninja 1.13.2、openocd 0.12.0
- Node.js v22（仅用于 `tools/gen_cat.js` 生成位图，不参与固件编译）

## 构建规则（最重要）

- **永远走全局构建**：`cmake --workflow --preset build`（= `Ctrl+Shift+B` 的 Build 任务）。
- **绝不**用"C/C++: 生成活动文件"（右上角 ▶）：单文件编译不带头文件路径与交叉编译选项，
  必然报 `stm32f1xx.h: No such file or directory`。
- 链接时 `_close/_read/_write not implemented` 是 nosys 桩的正常提示，忽略。
- 当前占用：Flash ≈ 7.2KB / 64KB，RAM ≈ 1.1KB / 20KB。
- **Ninja 路径不写死**：`CMakeLists.txt` 按 `NINJA_PATH`(指向 ninja 可执行文件)
  → `STM32_TOOLS/ninja/bin` → `~/stm32-tools/ninja/bin` → 系统 PATH 的顺序查找。
  换机器只需把 ninja 加进 PATH，或设 `NINJA_PATH` / `STM32_TOOLS` 环境变量。
  注意别用 `find_program` 找 ninja：交叉编译配置阶段它搜不到宿主程序，要用 `if(EXISTS ...)` 判定。
- 改了 `CMakePresets.json` 之后用 `cmake --fresh --preset stm32` 重建缓存，
  否则旧的 `CMAKE_MAKE_PROGRAM` 绝对路径会留在 `build/CMakeCache.txt` 里。

## 芯片与关键配置

- Cortex-M3，主频由 `main.c: sysclock_init()` 配到 72MHz（HSE 8MHz × 9），HSE 不起振则退回 HSI 8MHz
- 构建宏 `STM32F103xB`；`-mcpu=cortex-m3 -mthumb -Og -g3`；`nano.specs + nosys.specs + --gc-sections`
- 引脚全部集中在 `Core/Inc/pins.h`：
  I2C = PB6(SCL)/PB7(SDA)，EC11 = PA0/PA1(TIM2 编码器)，按键 = PA2(PSH)/PA3(CON)/PA4(BACK)
- 软件 I2C 用"开漏拉低 + 输入上拉释放"方式，避免模块没焊上拉导致通信失败
- OLED 地址 0x3C（`ssd1306.h` 的 `SSD1306_ADDR7`），刷新是全屏 1024 字节推送
- RTC：秒计数 = 自 2000-01-01 起的秒数，时钟源 LSE(32.768kHz) 优先、LSI 兜底；
  掉电保持靠 VBAT 电池
- **时间初值不要再手改 `rtc_time.h` 的 `DEFAULT_*`**：`CMakeLists.txt` 在每次 configure 时
  用 `string(TIMESTAMP ... UTC)` 把编译时刻注入 `RTC_BUILD_*` 宏，固件首次上电会把 RTC 设成
  "编译时刻 + `RTC_TZ_OFFSET_SEC`"。改时区改 `rtc_time.h` 里的 `RTC_TZ_OFFSET_SEC`（东八区 28800）。
  `DEFAULT_*` 只在宏没注入时兜底。
- 每次烧新固件：用 `BKP->DR2/DR3` 存的秒计数做指纹，与编译时刻不同才重写 CNT，
  所以正常复位不会覆盖已经走出来的时间。

## 烧录 / 调试

- **必须使用 `interface/stlink-hla.cfg`**（不是 `stlink.cfg`）：本机 ST-Link V2 固件是 **V2J23S4**，
  只支持旧版 HLA 协议；用 `stlink.cfg`（dapdirect）会报
  `The firmware in the ST-Link adapter only supports deprecated HLA` 并 init failed。
- 烧录：任务 **Flash (ST-Link)**（OpenOCD `program build/oled1.elf verify reset exit`）
- 调试：F5（cortex-debug + OpenOCD，gdb 为 `stm32-tools\gcc\bin\arm-none-eabi-gdb.exe`）
- 当前工程名 `oled1`，产物 `build/oled1.elf`（`tasks.json`/`launch.json` 按此路径引用）
- **OpenOCD 的 `-c` 参数里不能出现反斜杠**：TCL 会把 `\` 当转义符吃掉
  （`C:\Users\...` 会变成 `C:Users...` 然后 `couldn't open`）。
  因此 Flash 任务设置了 `cwd = ${workspaceFolder}` 并只写相对路径 `build/oled1.elf`。
- 偶发 `Error: open failed`（USB 被上一次未完全释放的会话占用）：等 2 秒重试即可，不是硬件故障。

## 工程维护铁律

1. `.vscode` 文件夹名必须带点（被改成 `vscode` 则全部配置失效）；
2. 工程移动/改名后，先删 `build/` 再重新构建（缓存了旧绝对路径）；
3. 新增源文件：同时更新 `CMakeLists.txt` 的 `add_executable` 列表；
4. 新增头文件目录：更新 `CMakeLists.txt` 的 `target_include_directories`；
5. `Core/Src/cat_bitmap.c` / `Core/Inc/cat_bitmap.h` 是生成文件，改图请改 `tools/gen_cat.js`
   后 `node tools/gen_cat.js`（会打印 ASCII 预览确认效果）。
