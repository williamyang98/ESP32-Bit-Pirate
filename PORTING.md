## Details on how Bit Pirate was ported from the ESP32-S3 to the ESP32
- Specifically ported to esp32-d0wd-v3.1
- Must fit inside 4MB flash and 200kB IRAM (not resizable)
- Change GPIO pin and reserved pin defines in ```platformio.ini``` to correct GPIO pins on ESP32

### Compiling
- Compile on WSL or Linux (Ubuntu)
- Do not compile on Windows you will get ```too long commands``` error for the linker since the generated linker script is too massive, whereas on WSL2 and Linux there are no issues with this
    - Also compiling on Windows is horrible since it keeps interfering either with file indexing or defender scanning or sandboxing every cmake/xtensa-elf-gcc/ld call slowing everything down
    - Symptoms include esp-idf taking forever to configure cmake and start compiling

### Disabling USB and other unsupported features in ESP32 compared to ESP32-S3
- Disabled USB features are enclosed by #ifndef NO_HARDWARE_USB
    - UART to Serial bridge
    - JTAG
- ESP32 and ESP32-S3 incompatibilities are enclosed by #ifndef DEVICE_CUSTOM
    - Temperature sensor deprecated for original ESP32 chip which was unreliable and inaccurate

### Reducing IRAM usage
- ESP32 unlike ESP32-S3 has a fixed 200kB IRAM
- This requires us to build esp-idf instead of relying on prebuilt binaries done by adding platform = espidf to ```platformio.ini```
- espressif official guide for [Optimizing IRAM Usage](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/performance/ram-usage.html#optimizing-iram-usage)
- Using esp-idf menuconfig to move code out of IRAM into flash which updates ```sdkconfig.custom```
- Either by disabling IRAM optimisations or explicitly moving code from IRAM to flash

```code
CONFIG_RINGBUF_PLACE_FUNCTIONS_INTO_FLASH=y
# CONFIG_ESP_WIFI_IRAM_OPT is not set
# CONFIG_ESP_WIFI_RX_IRAM_OPT is not set
CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH=y
# CONFIG_ESP32_WIFI_IRAM_OPT is not set
# CONFIG_ESP32_WIFI_RX_IRAM_OPT is not set
```

### Disabling watchdog timer idle task on CPU1
- Arduino libraries are blocking and will starve the watchdog timer for the RTOS idle task on CPU 1
- Disable this in esp-idf menuconfig to update ```sdkconfig.custom```

```code
# Watchdog timer configuration
# CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU1 is not set

# Arduino configuration
CONFIG_ENABLE_ARDUINO_DEPENDS=y
CONFIG_AUTOSTART_ARDUINO=y
# CONFIG_ARDUINO_RUN_CORE0 is not set
CONFIG_ARDUINO_RUN_CORE1=y
```

### Specific changes for ESP32 board
- BOYA flash is explicitly supported with ```SPI_FLASH_SUPPORT_BOYA_CHIP=y``` in ```sdkconfig.custom```
- Also disabling bug fixes for older revisions of the ESP32 chip since we know we have the ESP32-D0WD-V3.1
    - Version 3.1 of the chip has hardware fixes for the SPI flash which means we don't need to store the patch code in IRAM
    - Compiled binary will only support minimum revision of ESP32 but save flash and IRAM size [CONFIG_ESP32_REV_MIN](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig-reference.html#config-esp32-rev-min)
    - ```CONFIG_ESP32_REV_MIN_3=y```