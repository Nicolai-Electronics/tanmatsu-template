# Tanmatsu template app project

This template project shows how to build an app for Tanmatsu using the [PAX graphics](https://github.com/robotman2412/pax-graphics/tree/release/1.1.1/docs) library.

For more information visit the [documentation website](https://docs.tanmatsu.cloud).

## Disabling Bluetooth (BLE)

This template brings up the NimBLE Bluetooth LE host automatically in `main/main.c` (right after the WiFi/radio link is started), so that the app is ready for you to add your own GAP/GATT services and start advertising. If your app does not use BLE, you can remove it to save flash space and RAM:

1. Remove the `#if CONFIG_BT_NIMBLE_ENABLED ... #endif` block of BLE includes near the top of `main/main.c`, the `ble_host_task`/`ble_on_reset`/`ble_on_sync`/`ble_init` functions, and the `#if CONFIG_BT_NIMBLE_ENABLED ... #endif` block in `app_main()` that calls `ble_init()`.
2. Remove `bt` from `PRIV_REQUIRES` in `main/CMakeLists.txt`.
3. Disable Bluetooth in menuconfig (`make menuconfig DEVICE=<your device>` or `idf.py menuconfig`): turn off `Component config -> Bluetooth -> Bluetooth`, which disables NimBLE as well.

Note: do not simply remove the `ble_init()` call while leaving `CONFIG_BT_NIMBLE_ENABLED` on in sdkconfig — on targets where BT is provided by a co-processor over esp-hosted (e.g. Tanmatsu's ESP32-P4), the companion chip can still send BT HCI traffic up the shared link as soon as the WiFi/hosted link comes up. If NimBLE's host was never started, its receive buffers don't exist yet and the firmware will crash (`Load access fault` in `os_memblock_get`/`try_alloc_evt`) the first time such traffic arrives. Either keep `ble_init()` in place, or disable Bluetooth entirely as described above.

## License

The contents of this repository may be considered in the public domain or [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0) licensed at your disposal.

At Nicolai Electronics we love open source so we recommend licensing your work based on this template under terms of the [MIT license](https://opensource.org/license/mit). The MIT license allows others to build upon your work without restrictions while also making sure you retain your attribution.
