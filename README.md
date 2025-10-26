# Gellan Turbo 3000 - Multi-Channel Temperature Controller

## Project Description

This is firmware for ESP32 or ESP8266 microcontrollers that implements, 7-channel temperature controller with a web interface. The system is designed for laboratory applications that require reaching a precise temperature threshold, holding it for a specified duration, and then executing a controlled, linear cooling ramp.

The web interface allows for live monitoring of all seven samples and dynamic adjustment of process parameters for each channel independently.

## Core Features

* **7 Independent Channels**: Control seven separate heaters and sensors.
* **Web Interface**: Accessible from any browser on the local network, with live data updates (AJAX).
* **Programmable Process**: Set the temperature threshold, hold duration, and cooling ramp speed.
* **State Machine**: Each channel operates in one of three states: `Idle`, `Holding`, or `Cooling`.
* **Reliability**: Built-in hysteresis prevents rapid relay chattering.

## Hardware Requirements

* An ESP32 or ESP8266 module (e.g., NodeMCU).
* 7x **DS18B20** digital temperature sensors.
* 7x Relay modules or SSRs (Solid State Relays) to control the heaters.
* 1x **4.7kΩ** pull-up resistor for the OneWire bus.
* A suitable power supply for the ESP and a separate, more powerful supply for the heaters.

---

##  Pinout & Wiring Instructions

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

## 🚀 Getting Started (Step-by-Step)

### 1. Install Libraries

Open the Library Manager in your Arduino IDE and install:
* `ESPAsyncWebServer`
* `AsyncTCP` (if using ESP32)
* `ESPAsyncTCP` (if using ESP8266)
* `OneWire`
* `DallasTemperature`
* `ArduinoJson`

### 2. Configure WiFi

At the top of the `.ino` file (included below), find and change these lines to match your local WiFi network:

```cpp
const char* ssid = "YourNetworkName";
const char* password = "YourWiFiPassword";
```

###  3. Find Sensor Addresses (CRITICAL STEP)

Each DS18B20 sensor has a unique 64-bit address. The system will not work if the addresses in the code do not match your physical sensors.

How to find the addresses:

    Connect only one DS18B20 sensor to the ESP (to GPIO 4 with the pull-up resistor).

    Upload a simple "OneWireScanner" sketch (available in the examples of the DallasTemperature or OneWire library).

    Open the Serial Monitor and copy the address (e.g., {0x28, 0x3F, 0x4C, ...}).

    Repeat this process for all 7 sensors, noting which address corresponds to which sample.

Once you have all 7 addresses, paste them into the sensorAddresses array in the main .ino file (included below):
C++

```cpp
DeviceAddress sensorAddresses[NUM_SENSORS] = {
  {0x28, 0x3F, 0x4C, 0xDA, 0x05, 0x00, 0x00, 0x30}, // Paste Sensor 1 address here
  {0x28, 0x70, 0x40, 0x43, 0xD4, 0xAF, 0x15, 0xD4}, // Paste Sensor 2 address here
  // ...and so on for all 7
};
```
You can also update the sensorNames array to match your experiment.

### 4. Upload and Use

Copy the code from the section below into a new sketch in your Arduino IDE (e.g., GellanTurbo3000.ino).
Select the correct board (ESP32 or ESP8266) and COM port in the Arduino IDE.
Upload the code to your device.
Open the Serial Monitor (baud rate 115200).
After the ESP connects to your WiFi, it will print its IP address:
ESP IP Address: [http://192.168.1.123](http://192.168.1.123)
Type this IP address into a web browser on a computer or phone connected to the same WiFi network.
You can now monitor and control the process from the web interface!
