cmake_minimum_required(VERSION 3.21)

# 交叉编译工具链由 CMakePresets.json 指定（cmake/gcc-arm-none-eabi.cmake）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- 定位 Ninja（不在 presets 里写死路径，换机器也能直接构建）---
# 优先级：环境变量 NINJA_PATH(指向 ninja 可执行文件)
#        > 环境变量 STM32_TOOLS/ninja/bin
#        > ~/stm32-tools/ninja/bin
#        > 系统 PATH
set(_ninja_candidates "")

if(DEFINED ENV{NINJA_PATH} AND NOT "$ENV{NINJA_PATH}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{NINJA_PATH}" _p)
    list(APPEND _ninja_candidates "${_p}" "${_p}/ninja.exe" "${_p}/ninja")
endif()

if(DEFINED ENV{STM32_TOOLS} AND NOT "$ENV{STM32_TOOLS}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{STM32_TOOLS}" _t)
    list(APPEND _ninja_candidates "${_t}/ninja/bin/ninja.exe" "${_t}/ninja/bin/ninja")
endif()

# Windows 看 USERPROFILE，Linux/macOS 看 HOME；环境变量里的反斜杠要先转成 CMake 风格
foreach(_home_var USERPROFILE HOME)
    if(DEFINED ENV{${_home_var}} AND NOT "$ENV{${_home_var}}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{${_home_var}}" _home)
        list(APPEND _ninja_candidates "${_home}/stm32-tools/ninja/bin/ninja.exe"
                                      "${_home}/stm32-tools/ninja/bin/ninja")
    endif()
endforeach()

# 逐个用 EXISTS 判定：交叉编译配置阶段 find_program 搜不到宿主程序，用它会漏判
set(_ninja_prog "")
foreach(_c IN LISTS _ninja_candidates)
    if(EXISTS "${_c}" AND NOT IS_DIRECTORY "${_c}")
        set(_ninja_prog "${_c}")
        break()
    endif()
endforeach()

# 找到了就显式指定；没找到就保持默认，让 CMake 自己沿 PATH 找（找不到它会自己报错）
if(_ninja_prog)
    set(CMAKE_MAKE_PROGRAM "${_ninja_prog}" CACHE FILEPATH "Ninja 可执行文件" FORCE)
    message(STATUS "Ninja: ${_ninja_prog}")
else()
    message(STATUS "Ninja: 未在 NINJA_PATH / ~/stm32-tools 下找到，回退到 CMake 默认的 PATH 查找")
endif()

project(oled1 C ASM)

set(MCU_FLAGS -mcpu=cortex-m3 -mthumb)

add_executable(${PROJECT_NAME}
    # --- CMSIS / 启动 ---
    Core/Startup/startup_stm32f103xb.s
    Core/Src/system_stm32f1xx.c
    # --- 应用 ---
    Core/Src/main.c
    Core/Src/delay.c
    Core/Src/i2c_soft.c
    Core/Src/ssd1306.c
    Core/Src/font5x7.c
    Core/Src/rtc_time.c
    Core/Src/uart_frame.c
    Core/Src/ec11.c
    Core/Src/keys.c
    Core/Src/ui.c
    Core/Src/cat_bitmap.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32F1xx/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32F103xB
)

# --- 编译时刻（UTC）注入固件 ---
# 烧录后首次上电会把 RTC 设成这个值，再按 RTC_TZ_OFFSET_SEC 偏移成本地时间。
# 这样每次重新编译烧录时间都是新的，不用手改 rtc_time.h 里的 DEFAULT_*。
string(TIMESTAMP _bt_year "%Y" UTC)
string(TIMESTAMP _bt_mon  "%m" UTC)
string(TIMESTAMP _bt_day  "%d" UTC)
string(TIMESTAMP _bt_hour "%H" UTC)
string(TIMESTAMP _bt_min  "%M" UTC)
string(TIMESTAMP _bt_sec  "%S" UTC)
# 去掉前导零，否则 C 里 "08" 会被当成非法八进制
foreach(_v _bt_mon _bt_day _bt_hour _bt_min _bt_sec)
    string(REGEX REPLACE "^0+([0-9])" "\\1" ${_v} "${${_v}}")
endforeach()
target_compile_definitions(${PROJECT_NAME} PRIVATE
    RTC_BUILD_YEAR=${_bt_year}
    RTC_BUILD_MON=${_bt_mon}
    RTC_BUILD_DAY=${_bt_day}
    RTC_BUILD_HOUR=${_bt_hour}
    RTC_BUILD_MIN=${_bt_min}
    RTC_BUILD_SEC=${_bt_sec}
)
message(STATUS "RTC build time (UTC): ${_bt_year}-${_bt_mon}-${_bt_day} ${_bt_hour}:${_bt_min}:${_bt_sec}")

target_compile_options(${PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    -Og -g3
    -Wall -Wextra
    -ffunction-sections -fdata-sections
)

target_link_options(${PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    -specs=nano.specs
    -specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -T ${CMAKE_CURRENT_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld
)

# 编译完成后生成 .hex/.bin 并打印 Flash/RAM 占用
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generating .hex/.bin and printing size"
    VERBATIM
)

# 输出文件统一带 .elf 后缀（tasks.json / launch.json 按此引用 build/oled1.elf）
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")
