# Gellan Turbo 3000 - Multi-Channel Temperature Controller

## 📖 Project Description

This is firmware for ESP32 or ESP8266 microcontrollers that implements an advanced, 7-channel temperature controller with a web interface. The system is designed for laboratory applications (e.g., controlled gellan gum gelation) that require reaching a precise temperature threshold, holding it for a specified duration, and then executing a controlled, linear cooling ramp.

The web interface allows for live monitoring of all seven samples and dynamic adjustment of process parameters for each channel independently.

## ⚙️ Core Features

* **7 Independent Channels**: Control seven separate heaters and sensors.
* **Web Interface**: Accessible from any browser on the local network, with live data updates (AJAX).
* **Programmable Process**: Set the temperature threshold, hold duration, and cooling ramp speed.
* **State Machine**: Each channel operates in one of three states: `Idle`, `Holding`, or `Cooling`.
* **Reliability**: Built-in hysteresis prevents rapid relay chattering.

## 🔌 Hardware Requirements

* An ESP32 or ESP8266 module (e.g., NodeMCU).
* 7x **DS18B20** digital temperature sensors.
* 7x Relay modules or SSRs (Solid State Relays) to control the heaters.
* 1x **4.7kΩ** pull-up resistor for the OneWire bus.
* A suitable power supply for the ESP and a separate, more powerful supply for the heaters.

---

## 📌 Pinout & Wiring Instructions

This is the most critical section for getting the system to work. Pins have been selected to avoid conflicts with the Serial port (used for debugging).

### 1. Temperature Sensors (DS18B20)

All 7 DS18B20 sensors are connected in parallel to a single OneWire bus:

* **DATA Pin (DQ)**: Connect all sensor DATA pins together to **GPIO 4** (D2 on NodeMCU).
* **VCC**: Connect all sensor VCC pins to **3.3V**.
* **GND**: Connect all sensor GND pins to **GND**.
* **Pull-up Resistor (IMPORTANT!)**: You must connect a **4.7kΩ** resistor between the DATA pin (GPIO 4) and the VCC pin (3.3V).

### 2. Heater Control (Relays)

Each digital output controls a relay module. Connect the relay module's signal pin (IN) to the corresponding ESP pin.

* `Heater 1 (Gellan Gum)`: **GPIO 2** (D4 on NodeMCU)
* `Heater 2 (Sample 1)`: **GPIO 5** (D1 on NodeMCU)
* `Heater 3 (Sample 2)`: **GPIO 14** (D5 on NodeMCU)
* `Heater 4 (Sample 3)`: **GPIO 12** (D6 on NodeMCU)
* `Heater 5 (Sample 4)`: **GPIO 16** (D0 on NodeMCU)
* `Heater 6 (Sample 5)`: **GPIO 15** (D8 on NodeMCU)
* `Heater 7 (Sample 6)`: **GPIO 13** (D7 on NodeMCU)

### Summary Table (for NodeMCU ESP8266)

| Function | GPIO Pin | NodeMCU Label |
| :--- | :---: | :---: |
| **OneWire Bus (DS18B20)** | **GPIO 4** | **D2** |
| *4.7kΩ Pull-up* | *Between D2 & 3.3V* | |
| Heater Output 1 | **GPIO 2** | **D4** |
| Heater Output 2 | **GPIO 5** | **D1** |
| Heater Output 3 | **GPIO 14** | **D5** |
| Heater Output 4 | **GPIO 12** | **D6** |
| Heater Output 5 | **GPIO 16** | **D0** |
| Heater Output 6 | **GPIO 15** | **D8** |
| Heater Output 7 | **GPIO 13** | **D7** |

*(For ESP32, the GPIO numbers typically match the labels on the board, e.g., GPIO 4 is the pin labeled "4").*

---

## 🚀 Setup & Upload Guide (Step-by-Step)

This guide assumes you are using the **Arduino IDE**.

### Step 1: Configure Arduino IDE (One-Time Setup)

Before you can upload, you must teach the Arduino IDE how to work with ESP boards.

1.  Open the Arduino IDE.
2.  Go to `File` > `Preferences`.
3.  In the **"Additional Board Manager URLs"** field, paste **both** of the following links (separated by a comma):
    ```
    [http://arduino.esp8266.com/stable/package_esp8266com_index.json](http://arduino.esp8266.com/stable/package_esp8266com_index.json), [https://dl.espressif.com/dl/package_esp32_index.json](https://dl.espressif.com/dl/package_esp32_index.json)
    ```
4.  Click `OK`.
5.  Go to `Tools` > `Board: ...` > `Boards Manager...`.
6.  In the search bar, type `esp8266` and click `Install` on the result.
7.  In the search bar, type `esp32` and click `Install` on the result.
8.  Close the Boards Manager window.

### Step 2: Install Required Libraries

This code requires several libraries to function.

1.  Go to `Sketch` > `Include Library` > `Manage Libraries...`.
2.  In the search bar, find and **install** each of the following libraries:
    * `ESPAsyncWebServer`
    * `OneWire`
    * `DallasTemperature`
    * `ArduinoJson`
3.  **Important extra step:** `ESPAsyncWebServer` has a dependency.
    * **If you are using an ESP8266**, install `ESPAsyncTCP`.
    * **If you are using an ESP32**, install `AsyncTCP`.

### Step 3: Prepare and Upload the Code

1.  Create a new sketch in the Arduino IDE (`File` > `New`).
2.  Copy the entire code from the **"💾 Full Source Code"** section below and paste it into the Arduino IDE window.
3.  **Configure WiFi**: At the top of the code, change these lines to match your WiFi network:
    ```cpp
    const char* ssid = "YourNetworkName";
    const char* password = "YourWiFiPassword";
    ```
4.  **Configure Sensors (Critical!)**:
    * You must find the unique addresses of your DS18B20 sensors (use a "OneWireScanner" sketch from the `DallasTemperature` library examples).
    * Paste your sensor addresses into the `sensorAddresses` array in the code. The code will not read any temperatures without this.
    ```cpp
    DeviceAddress sensorAddresses[NUM_SENSORS] = {
      {0x28, 0x3F, 0x4C, 0xDA, 0x05, 0x00, 0x00, 0x30}, // <-- PASTE YOUR SENSOR 1 ADDRESS
      {0x28, 0x70, 0x40, 0x43, 0xD4, 0xAF, 0x15, 0xD4}, // <-- PASTE YOUR SENSOR 2 ADDRESS
      // ...and so on for all 7
    };
    ```
5.  **Connect Your Board**: Plug your ESP board into your computer with a USB data cable.
6.  **Select Board & Port**:
    * `Tools` > `Board: ...` > Select your board (e.g., `ESP8266 Boards` > `NodeMCU 1.0 (ESP-12E Module)`).
    * `Tools` > `Port:` > Select the COM port that appeared.
7.  **Upload the Code**: Click the "Upload" button (the right-arrow icon →).
    * *If the upload gets stuck on `Connecting........_____`*, try holding down the `BOOT` (or `FLASH`) button on your board just as the upload process begins.

### Step 4: Verify Success

1.  After the upload finishes, open the Serial Monitor: `Tools` > `Serial Monitor`.
2.  In the bottom-right corner of the Serial Monitor, set the baud rate to **115200**.
3.  Press the `RESET` (or `EN`) button on your ESP board.

You should see startup messages, followed by the IP address:
