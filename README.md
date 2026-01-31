# Duplo Train 2025

Basic code to connect Bluepad32 controller library
with Lego DUPLO Train models released in 2025
(LEGO part numbers 10427 or 10428)

Works in Arduino IDE

### Dependencies:
- Bluepad32 library
- NimBLEDevice (I used version 1.4.3)

### History:
I tried using the Legoino library but most of the examples are broken in 2025 due to newer dependency versions introducing breaking changes; I guess it hasn't been updated in a while.

As set up now, it works with 2x 8bitdo controllers and 2x Duplo Trains. Each controller can control both trains (L and R sticks etc.)

### Instructions:
1. Install **_TwoPads.ino** on to an original ESP32 as it has BT classic. It runs Bluepad32 and sends controller outputs over the serial port.

2. Install **_XYsticks.ino** on to any ESP32 variant (even -S3) it uses BLE to connect to and control the DUPLO Trains


