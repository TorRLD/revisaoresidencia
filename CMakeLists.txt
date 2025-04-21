set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(revisaoresidencia C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(revisaoresidencia revisaoresidencia.c include/ssd1306.c)

pico_set_program_name(revisaoresidencia "revisaoresidencia")
pico_set_program_version(revisaoresidencia "0.1")

# Generate PIO header
pico_generate_pio_header(revisaoresidencia ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(revisaoresidencia 0)
pico_enable_stdio_usb(revisaoresidencia 1)

# Add the standard library to the build
target_link_libraries(revisaoresidencia
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(revisaoresidencia PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(revisaoresidencia 
        hardware_i2c
        hardware_pio
        hardware_timer
        hardware_clocks
        hardware_adc
        hardware_pwm
        
        )

pico_add_extra_outputs(revisaoresidencia)

