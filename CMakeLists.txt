cmake_minimum_required(VERSION 3.21)

# 交叉编译工具链由 CMakePresets.json 指定（cmake/gcc-arm-none-eabi.cmake）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
