---
id: troubleshooting
title: Troubleshooting
sidebar_title: Troubleshooting
---
### Summary

The following page provides suggestions for common errors that may occur during firmware compilation. If the information provided is insufficient to resolve the issue, feel free to seek out help from the [ZMK Discord](https://zmkfirmware.dev/community/discord/invite).

### File Transfer Error

Variations of the warnings shown below occur when flashing the `<firmware>.uf2` onto the microcontroller. This is because the microcontroller resets itself before the OS receives confirmation that the file transfer is complete. Errors like this are normal and can generally be ignored. Verification of a functional board can be done by attempting to pair your newly flashed keyboard to your computer via Bluetooth or plugging in a USB cable if `ZMK_USB` is enabled in your Kconfig.defconfig.

|       ![Example Error Screen](../docs/assets/troubleshooting/filetransfer/windows.png)      |
| :-------------------------------------------------------------------------------:           |
|            An example of the file transfer error on Windows 10                              |

|       ![Example Error Screen](../docs/assets/troubleshooting/filetransfer/linux.png)        |
| :-------------------------------------------------------------------------------:           |
|           An example of the file transfer error on Linux                                    |


### CMake Error

```
CMake Warning at C:/zmk/zephyr/subsys/usb/CMakeLists.txt:28 (message):
  CONFIG_USB_DEVICE_VID has default value 0x2FE3.
 
  This value is only for testing and MUST be configured for USB products.
 
 
CMake Warning at C:/zmk/zephyr/subsys/usb/CMakeLists.txt:34 (message):
  CONFIG_USB_DEVICE_PID has default value 0x100.
 
  This value is only for testing and MUST be configured for USB products.
```

CMake Warnings shown above during `west build` are normal occurrences. They should not negatively affect the firmware's ability to function as normal.

On the other hand, an error along the lines of `CMake Error at (zmk directory)/zephyr/cmake/generic_toolchain.cmake:64 (include): include could not find load file:` during firmware compilation indicates that the Zephyr Environment Variables are not properly defined.
For more information, click [here](../docs/dev-setup#environment-variables).


### dtlib.DTError

An error along the lines of `dtlib.DTError: <board>.dts.pre.tmp:<line number>` during firmware compilation indicates an issue within the `<shield>.keymap` file.
This can be verified by checking the file in question, found in `mkdir/app/build`.

|       ![Example Error Screen](../docs/assets/troubleshooting/keymaps/errorscreen.png)                                             |
| :-------------------------------------------------------------------------------:                                                 |
|            An example of the dtlib.DTError when compiling an iris with the nice!nano while the keymap is not properly defined     |

After opening the `<board>.dts.pre.tmp:<line number>` and scrolling down to the referenced line, one can locate errors within their shield's keymap by checking if the referenced keycodes were properly converted into the correct [USB HID Usage ID](https://www.usb.org/document-library/hid-usage-tables-12).

|       ![Unhealthy Keymap Temp](../docs/assets/troubleshooting/keymaps/unhealthyEDIT.png)  |
| :-------------------------------------------------------------------------------:         |
|   An incorrectly defined keymap unable to compile. As shown in red, `&kp SPAC` is not a valid reference to the [USB HID Usage ID](https://www.usb.org/document-library/hid-usage-tables-12) used for "Keyboard Spacebar"         |

|  ![Healthy Keymap Temp](../docs/assets/troubleshooting/keymaps/healthyEDIT.png)  |
| :-------------------------------------------------------------------------------: |
|  A properly defined keymap with successful compilation. As shown in red, the corrected keycode (`&kp SPC`) references the proper Usage ID defined in the [USB HID Usage Tables](https://www.usb.org/document-library/hid-usage-tables-12)|
