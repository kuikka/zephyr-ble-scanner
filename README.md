# zephyr-ble-scanner
Based on samples/scan_adv,cdc_acm

# Building
$env:ZEPHYR_BASE="C:\Dev\zephyr\zephyrproject\zephyr"
$env:ZEPHYR_TOOLCHAIN_VARIANT="gnuarmemb"
$env:GNUARMEMB_TOOLCHAIN_PATH="C:\Dev\toolchains\gcc-arm-none-eabi-9-2019-q4-major"
west build