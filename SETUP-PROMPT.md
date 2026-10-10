# STM32F103C8T6 × VSCode 开发环境 —— 一键重建提示词

> **用途**：换新电脑（Windows x64）时，把整个工程文件夹拷贝过去（U 盘/网盘/git 均可，
> 只有本文件也行），然后在 ZCode（或任意 AI 编程助手）里发送这句话：
>
> ```
> 读取并严格按照工程里的 SETUP-PROMPT.md 执行，在我的新电脑上重建
> STM32F103C8T6 的 VSCode 开发环境（编译+烧录+调试），全部验收通过后汇报。
> ```
>
> 执行者注意：按顺序做，每步有验收；第 5 步分 A/B 两种情况，先判断工程文件夹是否完整。

---

## 0. 目标与最终验收标准

在新 Windows 机器上产出：

1. 四个命令行工具装到 `%USERPROFILE%\stm32-tools`（gcc / cmake / ninja / openocd），版本命令可运行；
2. 工程文件夹（默认名 `stm32f103c8t6-template`）内 `cmake --workflow --preset build` 从零成功；
3. `build/` 下生成 `stm32f103c8t6.elf / .hex / .bin` 三件套，`readelf -h` 显示 `Machine: ARM`；
4. VSCode 打开工程文件夹后：`Ctrl+Shift+B` 全局编译、`Flash (ST-Link)` 任务烧录、`F5` 断点调试全部可用；
5. 五个 VSCode 扩展已安装；
6. 用户 PATH 已追加四个工具的 bin 目录。

## 1. 前置检查（先摸底再动手）

```bash
for t in arm-none-eabi-gcc cmake ninja openocd code winget python; do
  command -v "$t" >/dev/null 2>&1 && echo "$t 已有" || echo "$t 缺"
done
curl -sI --max-time 8 https://api.github.com | head -n1   # 网络是否通
```

- 已有的工具（版本 ≥ 要求即可）不必重装：cmake ≥ 3.25、ninja 任意、其余见下表；
- `code` 命令不可用 → 让用户从 VSCode 内运行 "Shell Command: Install 'code' in PATH"，或直接用
  VSCode 安装目录下的 `bin/code`。

## 2. 安装四个工具（全部装到 `%USERPROFILE%\stm32-tools`）

| 工具 | GitHub 仓库（xpack-dev-tools/…） | 参考版本 | 解压后重命名为 |
|---|---|---|---|
| ARM GCC + GDB | `arm-none-eabi-gcc-xpack` | 15.2.1-1.1 | `gcc` |
| CMake | `cmake-xpack` | 3.31.9-1 | `cmake` |
| Ninja | **`ninja-build-xpack`**（不叫 ninja-xpack） | 1.13.1-1 | `ninja` |
| OpenOCD | `openocd-xpack` | 0.12.0-7 | `openocd` |

**下载规则（都是实战踩出来的）：**

1. 用 GitHub API 拿最新版地址：
   `https://api.github.com/repos/xpack-dev-tools/<repo>/releases/latest`
2. Windows 资源文件名是 `xpack-<名>-<版本>-win32-x64.zip`——**是 .zip，不是 .tar.gz**（最新
   release 的 tar.gz 只有 macOS/Linux，按 tar.gz 过滤会全空）；
3. 直连易断（exit 56 连接重置），用镜像前缀 + 断点续传：
   ```bash
   url="<browser_download_url>"
   for base in "https://ghfast.top/" "https://gh-proxy.com/" ""; do
     curl -sL --fail -C - --retry 8 --retry-delay 3 --retry-all-errors \
          --max-time 580 -o pkg.zip "${base}${url}" && break
   done
   ```
4. 校验完整性：`/c/Windows/System32/tar.exe -tf pkg.zip >/dev/null` 无报错才算完整；
5. 解压到 `%USERPROFILE%\stm32-tools`：`/c/Windows/System32/tar.exe -xf pkg.zip -C <目录>`，
   然后把 `xpack-XXX-版本` 目录 `mv` 成上表的短名；
6. 验证：四个 `<dir>/bin/*.exe --version` 各跑一遍。

## 3. 用户 PATH + ST-Link 检测（PowerShell）

```powershell
$add = "$env:USERPROFILE\stm32-tools\gcc\bin;$env:USERPROFILE\stm32-tools\cmake\bin;$env:USERPROFILE\stm32-tools\ninja\bin;$env:USERPROFILE\stm32-tools\openocd\bin"
$cur = [Environment]::GetEnvironmentVariable('Path','User')
$items = $cur -split ';' | Where-Object { $_ -ne '' }
$parts = $add -split ';' | Where-Object { $items -notcontains $_ }
if ($parts.Count -gt 0) { [Environment]::SetEnvironmentVariable('Path', (($items + $parts) -join ';'), 'User') }
# ST-Link 检测（没有也不阻塞，只提醒）
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'ST-?Link' }
```

## 4. VSCode 扩展

```bash
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.cmake-tools
code --install-extension marus25.cortex-debug
code --install-extension twxs.cmake
code --install-extension ms-vscode.vscode-serial-monitor
```

## 5. 工程文件夹

### 情况 A：工程文件夹完整随行（推荐）

工程文件全部随文件夹携带，**不需要重建任何源码**。只做两件事：

1. **替换旧机器的绝对路径**。以下文件里写死了 `C:\Users\huawei\stm32-tools`，全部替换成
   新机器的 `%USERPROFILE%\stm32-tools`（注意 json 里是 `\\` 双反斜杠或 `/` 正斜杠）：

   | 文件 | 出现位置 |
   |---|---|
   | `CMakePresets.json` | `CMAKE_MAKE_PROGRAM`（ninja 绝对路径） |
   | `.vscode/settings.json` | cmakePath / PATH 环境注入 / compilerPath / openocdPath |
   | `.vscode/tasks.json` | 三个任务的 command 绝对路径 |
   | `.vscode/launch.json` | serverpath / gdbPath |
   | `.vscode/c_cpp_properties.json` | compilerPath |

   （`cmake/gcc-arm-none-eabi.cmake` 用 `find_program` 自动找，无需改。）
2. **删除 `build/` 目录**（里面缓存了旧机器的绝对路径），再执行第 6 步验证。

### 情况 B：只有本文件 —— 从零重建工程

目录结构：

```
stm32f103c8t6-template/
├── CMakeLists.txt  CMakePresets.json  STM32F103C8Tx_FLASH.ld  .gitignore
├── AGENTS.md  SETUP-PROMPT.md  README.md
├── cmake/gcc-arm-none-eabi.cmake
├── Core/Src/main.c  Core/Src/system_stm32f1xx.c  Core/Startup/startup_stm32f103xb.s
├── Drivers/CMSIS/Include/{core_cm3.h, cmsis_version.h, cmsis_compiler.h, cmsis_gcc.h, mpu_armv7.h}
├── Drivers/CMSIS/Device/ST/STM32F1xx/Include/{stm32f1xx.h, stm32f103xb.h, system_stm32f1xx.h}
└── .vscode/{settings.json, tasks.json, launch.json, c_cpp_properties.json, extensions.json}
```

**官方文件下载源（两个仓库，别搞混）：**

- CMSIS 内核头文件（`Drivers/CMSIS/Include/` 下 5 个）来自
  `https://raw.githubusercontent.com/STMicroelectronics/STM32CubeF1/master/Drivers/CMSIS/Include/<文件名>`
- F1 设备头文件、`system_stm32f1xx.c`、启动文件来自 **独立仓库 cmsis-device-f1**
  （STM32CubeF1 里这些是子模块，直接按 CubeF1 路径拼会 404！）：
  `https://raw.githubusercontent.com/STMicroelectronics/cmsis-device-f1/master/Include/stm32f1xx.h`
  `…/master/Include/stm32f103xb.h`
  `…/master/Include/system_stm32f1xx.h`
  `…/master/Source/Templates/system_stm32f1xx.c`
  `…/master/Source/Templates/gcc/startup_stm32f103xb.s`
- raw.githubusercontent.com 断连时前面拼 `https://ghfast.top/`。
- 每个文件下载后核对大小：`stm32f103xb.h` 约 843KB、`core_cm3.h` 约 110KB，
  明显偏小就是下载失败。

**其余文件内容**（链接脚本、CMakeLists、.vscode 五件套、main.c）都是标准内容，
从本模板仓库/任一 STM32F103 CMake 模板抄即可，要点：
`STM32F103xB` 宏、`-mcpu=cortex-m3 -mthumb`、`--specs=nano.specs --specs=nosys.specs`、
链接脚本 Flash 64K@0x08000000 / RAM 20K@0x20000000、目标 SUFFIX 设为 `.elf`、
CMakePresets 用 version 6 且带 `workflowPresets`（configure+build 一条龙）。

## 6. 验证（全部命令与 VSCode 任务等价）

```bash
export PATH="$USERPROFILE/stm32-tools/gcc/bin:$USERPROFILE/stm32-tools/ninja/bin:$PATH"  # Git Bash 下按实际路径
cd <工程目录>
rm -rf build                                   # 必须从零开始验证
cmake --workflow --preset build                # 配置+编译一条命令
ls build | grep -E '\.(elf|hex|bin)$'          # 应有三件套
arm-none-eabi-readelf -h build/stm32f103c8t6.elf | grep Machine   # 应为 ARM
```

预期：blinky 工程约 Flash 596B / RAM 28B。
再跑一次同样的构建命令，输出 `ninja: no work to do.` 属于**正常的增量编译**，不是错误。

## 7. 已知坑速查表（全部踩过，按此排查）

| 症状 | 原因与解法 |
|---|---|
| `stm32f1xx.h: No such file or directory` | 用了"生成活动文件"（右上角 ▶）单文件编译。唯一正确入口：`Ctrl+Shift+B` → **Build** 任务 |
| VSCode 里没有 Build/Flash 任务可选 | `.vscode` 文件夹被改名成 `vscode`（丢了点）。改回 `.vscode` 后重开 |
| 移动/改名工程后编译报奇怪路径错误 | `build/` 缓存旧绝对路径。删 `build/` 再 `Ctrl+Shift+B` |
| GitHub release 资产列表全空 | Windows 包是 `.zip` 后缀，别按 `.tar.gz` 过滤 |
| ninja 仓库 404 | 仓库名是 `ninja-build-xpack` |
| CubeF1 里找不到 stm32f103xb.h (404) | F1 设备文件在独立仓库 `cmsis-device-f1` |
| 链接警告 `_close/_write is not implemented` | nosys 桩的正常提示，裸机工程无害，忽略 |
| OpenOCD 找不到 ST-Link | 装 ST 官方驱动 stsw-link009；仍不行用 Zadig 把 `ST-Link Debug` 换 WinUSB |
| 烧录报 `target examination failed` | 查 SWD 四线接线；BOOT0 跳线保持 0；按住 RESET 点烧录瞬间松开 |

其他事实：C8T6 官方标称 Flash 64K、实际颗粒多为 128K（链接脚本保守用 64K，可自行改）；
SVD 文件是可选增强（本次各源均不可用，未配，不影响断点调试）。

## 8. 烧录与调试（最终用户操作）

接线：ST-Link 的 3.3V/SWDIO/SWCLK/GND ↔ Blue Pill 的 3.3V/PA13/PA14/GND。

- **编译**：`Ctrl+Shift+B`
- **烧录**：`Ctrl+Shift+P` → Tasks: Run Task → `Flash (ST-Link)`（先自动编译，烧完复位运行）
- **调试**：`F5`（自动编译 → OpenOCD 连接 → 停在 `main()`）
- 命令行等价：`openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/stm32f103c8t6.elf verify reset exit"`

## 9. 收尾（必做）

1. 更新工程内 `AGENTS.md` 里记录的工具路径为新机器路径；
2. 提醒用户：**重启 VSCode** 让新 PATH 在集成终端里生效；
3. 汇报验收清单第 0 节 6 项的逐项结果。
