# STM32 交叉编译工具链文件（arm-none-eabi-gcc / Cortex-M3）
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 交叉编译时跳过可执行文件试编译
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(ARM_GCC_EXEC    arm-none-eabi-gcc     REQUIRED)
find_program(ARM_GXX_EXEC    arm-none-eabi-g++     REQUIRED)
find_program(ARM_OBJCOPY_EXE arm-none-eabi-objcopy REQUIRED)
find_program(ARM_SIZE_EXE    arm-none-eabi-size    REQUIRED)
find_program(ARM_GDB_EXEC    arm-none-eabi-gdb     REQUIRED)

set(CMAKE_C_COMPILER   ${ARM_GCC_EXEC})
set(CMAKE_CXX_COMPILER ${ARM_GXX_EXEC})
set(CMAKE_ASM_COMPILER ${ARM_GCC_EXEC})
set(CMAKE_OBJCOPY      ${ARM_OBJCOPY_EXE})
set(CMAKE_SIZE         ${ARM_SIZE_EXE})

# 启动汇编文件（.s）需要的 CPU 选项
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m3 -mthumb")
