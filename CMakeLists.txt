# Set compile definitions (macros)
add_compile_definitions(
    LUAT_HEAP_SIZE=150*1024
    CONFIG_I2C_SKIP_LEGACY_CONFLICT_CHECK=1
    DISABLE_ALL_LIBRARY_WARNINGS
)

# Uncomment these if needed later
# add_compile_definitions(ARDUINO_USB_CDC_ON_BOOT=0)
# add_compile_definitions(ARDUINO_USB_MODE=0)
# add_compile_definitions(BOARD_HAS_PSRAM)
# add_compile_definitions(LUAT_USE_PSRAM)
