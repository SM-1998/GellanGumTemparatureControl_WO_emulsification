/**
 * @brief A multi-channel temperature controller with web interface for ESP32/ESP8266.
 *
 * This firmware implements a 7-channel temperature controller designed for
 * laboratory processes (e.g., gellan gum gelation). It features a web-based UI
 * for real-time monitoring and parameter adjustment.
 *
 * Each channel follows a three-stage state machine:
 * 1. HOLD: Heats to a threshold, then holds for a set duration.
 * 2. COOL: Linearly ramps down the temperature setpoint at a defined rate.
 * 3. IDLE: Waits for conditions to restart the process.
 *
 * @author [Stanislaw Marecik/ReGenDDS]
 * @date [26.10.2025]
 */

//==============================================================================
// Includes
//==============================================================================
#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

//==============================================================================
// Configuration
//==============================================================================
const char* ssid = "WIFI SSID";       // <--- CHANGE TO YOUR WIFI SSID
const char* password = "PASSWORD"; // <--- CHANGE TO YOUR WIFI PASSWORD

const int NUM_SENSORS = 7;           // Number of sensors/channels to control
const float HYSTERESIS = 0.5;      // Hysteresis (in °C) to prevent output chattering

//==============================================================================
// Pin Definitions
//==============================================================================

// Output pins for controlling heaters/relays.
// NOTE: Pins 1 (TX) and 3 (RX) are avoided as they conflict with Serial debug.
// These pins (15=D8, 13=D7 on NodeMCU) are safe alternatives.
int outputPins[NUM_SENSORS] = {2, 5, 14, 12, 16, 15, 13};

// Input pin for the OneWire bus (all DS18B20 sensors connected here)
const int oneWireBus = 4; // D2 on NodeMCU

//==============================================================================
// Global Variables - Process Parameters
//==============================================================================
// These arrays hold the user-configurable settings for each channel.

/**
 * @brief Target temperature setpoint (°C).
 * @note This array is MODIFIED by the cooling ramp logic. It's the "live" setpoint.
 */
float thresholdTemps[NUM_SENSORS] = {60.0, 60.0, 60.0, 60.0, 60.0, 60.0, 60.0}; // <--- CHANGE THIS FOR YOUR TEMPERATURE

/**
 * @brief Rate of temperature decrease during the cooling phase (°C / minute).
 */
float coolingSpeeds[NUM_SENSORS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}; // <--- CHANGE THIS FOR YOUR TEMPERATURE

/**
 * @brief The minimum temperature setpoint to reach during the cooling ramp.
 */
float lowerLimits[NUM_SENSORS] = {37.0, 37.0, 37.0, 37.0, 37.0, 37.0, 37.0}; // <--- CHANGE THIS FOR YOUR TEMPERATURE

/**
 * @brief Duration (in minutes) to hold the temperature after reaching the threshold.
 */
unsigned long holdDurations[NUM_SENSORS] = {60, 60, 60, 60, 60, 60, 60}; // <--- CHANGE THIS FOR YOUR TIME

//==============================================================================
// Global Variables - System State
//==============================================================================
// These arrays track the real-time operational state of each channel.

float lastTemperatures[NUM_SENSORS];        // Stores the last valid temperature read
bool outputState[NUM_SENSORS] = {false};    // Current state of the output pin (HIGH/LOW)
bool holdPhaseActive[NUM_SENSORS] = {false};  // True if the 'Hold' phase is active
bool coolingPhaseActive[NUM_SENSORS] = {false}; // True if the 'Cooling' phase is active
unsigned long phaseStartMillis[NUM_SENSORS] = {0}; // Timestamp (millis()) when the last phase started

//==============================================================================
// Global Objects
//==============================================================================
AsyncWebServer server(80);
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

//==============================================================================
// Sensor Configuration
//==============================================================================
// Unique 64-bit addresses for each DS18B20 sensor.
// IMPORTANT: You must replace these with the addresses of your specific sensors.
// Use a "OneWireScanner" sketch to find these addresses.
DeviceAddress sensorAddresses[NUM_SENSORS] = {
  {0x28, 0x3F, 0x4C, 0xDA, 0x05, 0x00, 0x00, 0x30}, // <--- CHANGE THIS ADDRESS
  {0x28, 0x70, 0x40, 0x43, 0xD4, 0xAF, 0x15, 0xD4}, // <--- CHANGE THIS ADDRESS
  {0x28, 0xAC, 0xDC, 0x46, 0xD4, 0xB9, 0x2B, 0x9D}, // <--- CHANGE THIS ADDRESS
  {0x28, 0x0E, 0x2A, 0x45, 0xD4, 0x8D, 0x3A, 0xC8}, // <--- CHANGE THIS ADDRESS
  {0x28, 0xC5, 0x53, 0x46, 0xD4, 0xB0, 0x37, 0xE0}, // <--- CHANGE THIS ADDRESS
  {0x28, 0xDF, 0x12, 0x45, 0xD4, 0xC1, 0x1A, 0x74}, // <--- CHANGE THIS ADDRESS
  {0x28, 0xCD, 0x11, 0x46, 0xD4, 0xBF, 0x64, 0x0A}  // <--- CHANGE THIS ADDRESS
};

// User-friendly names for the web interface
String sensorNames[NUM_SENSORS] = {"Syringe", "Sample 1", "Sample 2", "Sample 3", "Sample 4", "Sample 5", "Sample 6"}; 


//==============================================================================
// Web Interface (HTML/CSS/JS)
//==============================================================================
// Stored in PROGMEM (Flash) to save precious RAM.
const char HTML_CONTENT[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Temperature Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; }
    table { border-collapse: collapse; width: 100%; }
    th, td { border: 1px solid #dddddd; text-align: left; padding: 8px; }
    th { background-color: #f2f2f2; }
    input[type=number] { width: 80px; }
    input[type=submit] { margin-top: 15px; padding: 10px 20px; font-size: 16px; cursor: pointer; }
    .status { padding: 5px; color: white; border-radius: 5px; text-align: center;}
    .status-ok { background-color: green; }
    .status-saving { background-color: orange; }
    .status-error { background-color: red; }
  </style>
</head>
<body>
  <h2>Gellan Turbo 3000</h2>
  <form id="controlForm">
    <table>
      <thead>
        <tr>
          <th>Name</th>
          <th>Temperature (&deg;C)</th>
          <th>Threshold (&deg;C)</th>
          <th>Cooling Speed (C/min)</th>
          <th>Lower Limit (&deg;C)</th>
          <th>Hold Duration (min)</th>
          <th>Time Remaining</th>
          <th>Status</th>
        </tr>
      </thead>
      <tbody id="sensor-table">
        </tbody>
    </table>
    <input type="submit" value="Save Changes">
    <div id="saveStatus" class="status"></div>
  </form>

<script>
  /**
   * Fetches the latest data from the /data endpoint and updates the table.
   */
  function updateSensorData() {
    fetch('/data')
      .then(response => response.json())
      .then(data => {
        if (Array.isArray(data)) {
            data.forEach((sensor, i) => {
            const tempEl = document.getElementById(`temp${i}`);
            const timeEl = document.getElementById(`time${i}`);
            const statusEl = document.getElementById(`status${i}`);

            if (tempEl) tempEl.innerText = sensor.temp.toFixed(2);
            if (timeEl) timeEl.innerText = sensor.time_rem;
            if (statusEl) statusEl.innerText = sensor.status;
          });
        }
      })
      .catch(error => console.error('Error fetching data:', error));
  }

  /**
   * Handles the form submission event.
   * Sends the new parameters to the /update endpoint via POST.
   */
  function handleFormSubmit(event) {
    event.preventDefault(); // Prevent default page reload
    const formData = new FormData(event.target);
    const statusDiv = document.getElementById('saveStatus');
    
    statusDiv.textContent = 'Saving...';
    statusDiv.className = 'status status-saving';

    fetch('/update', {
      method: 'POST',
      body: formData
    })
    .then(response => {
      if (response.ok) {
        statusDiv.textContent = 'Changes saved successfully!';
        statusDiv.className = 'status status-ok';
      } else {
        throw new Error('Server responded with an error');
      }
      // Clear status message after 3 seconds
      setTimeout(() => { statusDiv.textContent = ''; statusDiv.className = 'status'; }, 3000);
    })
    .catch(error => {
      console.error('Error submitting form:', error);
      statusDiv.textContent = 'Error saving changes.';
      statusDiv.className = 'status status-error';
    });
  }

  // --- Page Load Initialization ---
  window.addEventListener('load', () => {
    // Fetch initial data as soon as the page loads
    updateSensorData();
    
    // Set up a timer to periodically refresh the data every 2 seconds
    setInterval(updateSensorData, 2000);
    
    // Attach the submit handler to the form
    document.getElementById('controlForm').addEventListener('submit', handleFormSubmit);
  });
</script>
</body>
</html>
)rawliteral";

//==============================================================================
// Function: generateTableRows
//==============================================================================
/**
 * @brief  Dynamically generates the HTML table rows for the web interface.
 * @return A String object containing the complete HTML for all table rows.
 */
String generateTableRows() {
  String rows = "";
  for (int i = 0; i < NUM_SENSORS; i++) {
    rows += "<tr>";
    rows += "<td>" + sensorNames[i] + "</td>";
    rows += "<td id='temp" + String(i) + "'>-</td>"; // Placeholder for live temperature
    rows += "<td><input type='number' step='0.1' name='threshold" + String(i) + "' value='" + String(thresholdTemps[i]) + "'></td>";
    rows += "<td><input type='number' step='0.1' name='cooling" + String(i) + "' value='" + String(coolingSpeeds[i]) + "'></td>";
    rows += "<td><input type='number' step='0.1' name='lower" + String(i) + "' value='" + String(lowerLimits[i]) + "'></td>";
    rows += "<td><input type='number' step='1' name='hold" + String(i) + "' value='" + String(holdDurations[i]) + "'></td>";
    rows += "<td id='time" + String(i) + "'>-</td>";  // Placeholder for remaining time
    rows += "<td id='status" + String(i) + "'>-</td>"; // Placeholder for current status
    rows += "</tr>";
  }
  return rows;
}


//==============================================================================
// Function: setup
//==============================================================================
/**
 * @brief Main initialization function. Runs once on boot.
 * @details Initializes Serial, WiFi, Pins, Sensors, and configures
 * all web server endpoints.
 */
void setup() {
  Serial.begin(115200);

  // --- WiFi Connection ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return; // Halt execution if WiFi fails
  }
  Serial.print("ESP IP Address: http://");
  Serial.println(WiFi.localIP());

  // --- Hardware Initialization ---
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(outputPins[i], OUTPUT);
    digitalWrite(outputPins[i], LOW); // Ensure all outputs are off on boot
  }
  sensors.begin(); // Initialize the DallasTemperature library

  //=======================================
  // --- Web Server Endpoints ---
  //=======================================

  /**
   * @brief Serves the main HTML page.
   * This endpoint constructs the page by taking the PROGMEM template
   * and injecting the dynamically generated table rows.
   */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = FPSTR(HTML_CONTENT);
    // Find the placeholder and replace it with actual HTML
    html.replace("", generateTableRows());
    request->send(200, "text/html", html);
  });

  /**
   * @brief Serves real-time sensor data as a JSON array.
   * This endpoint is called by the JavaScript 'fetch' function every 2 seconds.
   * It builds a JSON array where each object represents a sensor's current state.
   */
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Use StaticJsonDocument for fixed size, preventing memory fragmentation
    StaticJsonDocument<1536> doc;
    // The root of the response must be an array for 'data.forEach' in JS
    JsonArray sensors_array = doc.to<JsonArray>();

    for (int i = 0; i < NUM_SENSORS; i++) {
      JsonObject sensor = sensors_array.add<JsonObject>();
      sensor["temp"] = lastTemperatures[i];
      
      String remainingStr = "-";
      String statusStr = "Idle";
      
      if (holdPhaseActive[i]) {
        unsigned long holdDurationSecs = holdDurations[i] * 60;
        unsigned long elapsedSecs = (millis() - phaseStartMillis[i]) / 1000;
        
        if (elapsedSecs < holdDurationSecs) {
          unsigned long remainingSecs = holdDurationSecs - elapsedSecs;
          // Format time as M:SS for better readability
          unsigned long remMinutes = remainingSecs / 60;
          unsigned long remSeconds = remainingSecs % 60;
          remainingStr = String(remMinutes) + ":" + (remSeconds < 10 ? "0" : "") + String(remSeconds);
        }
        statusStr = "Holding";
        
      } else if (coolingPhaseActive[i]) {
        statusStr = "Cooling";
      }

      sensor["time_rem"] = remainingStr;
      sensor["status"] = statusStr;
    }

    // Serialize and send the JSON response
    String json_response;
    serializeJson(doc, json_response);
    request->send(200, "application/json", json_response);
  });

  /**
   * @brief Handles POST requests from the form to update settings.
   * This endpoint parses the form data submitted by the user and updates
   * the global process parameter arrays.
   */
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Iterate through all possible parameters
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (request->hasParam("threshold" + String(i), true)) {
        thresholdTemps[i] = request->getParam("threshold" + String(i), true)->value().toFloat();
      }
      if (request->hasParam("cooling" + String(i), true)) {
        coolingSpeeds[i] = request->getParam("cooling" + String(i), true)->value().toFloat();
      }
      if (request->hasParam("lower" + String(i), true)) {
        lowerLimits[i] = request->getParam("lower" + String(i), true)->value().toFloat();
      }
      if (request->hasParam("hold" + String(i), true)) {
        holdDurations[i] = request->getParam("hold" + String(i), true)->value().toInt();
      }
    }
    
    // Optional: Reset all logic to IDLE state after settings are saved.
    // This ensures a clean start with the new parameters.
    for(int i=0; i < NUM_SENSORS; i++) {
        holdPhaseActive[i] = false;
        coolingPhaseActive[i] = false;
        Serial.println("Sensor " + String(i) + ": Settings updated, cycle reset to Idle.");
    }
    
    request->send(200, "text/plain", "OK"); // Send a simple 'OK' response
  });

  // Handle 404 Not Found errors
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Not found");
  });

  // Start the web server
  server.begin();
}


//==============================================================================
// Function: loop
//==============================================================================
/**
 * @brief Main execution loop.
 * @details This loop is non-blocking. It uses millis() to schedule two
 * main tasks:
 * 1. Reading sensor data (infrequent, every 2s).
 * 2. Running the control logic state machine (frequent, every 500ms).
 */
void loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastSensorRead = 0;
  static unsigned long lastLogicUpdate = 0;

  // --- Task 1: Read Sensors (Interval: 2000ms) ---
  // This task requests temperatures from all sensors and stores them.
  if (currentMillis - lastSensorRead >= 2000) {
    lastSensorRead = currentMillis;
    
    // Issue a non-blocking request to all sensors on the bus
    sensors.requestTemperatures(); 
    
    // Retrieve the temperature for each sensor by its address
    for (int i = 0; i < NUM_SENSORS; i++) {
      float temp = sensors.getTempC(sensorAddresses[i]);
      
      if(temp != DEVICE_DISCONNECTED_C) {
        lastTemperatures[i] = temp;
      } else {
        lastTemperatures[i] = -127.0; // Use error value
        Serial.println("Error reading sensor " + String(i));
      }
    }
  }

  // --- Task 2: Control Logic (Interval: 500ms) ---
  // Run logic more frequently than sensor reads for better responsiveness.
  if (currentMillis - lastLogicUpdate >= 500) {
    unsigned long elapsedSinceUpdate = currentMillis - lastLogicUpdate;
    lastLogicUpdate = currentMillis;

    for (int i = 0; i < NUM_SENSORS; i++) {
      float temp = lastTemperatures[i];
      
      // Skip logic for this sensor if it's disconnected
      if (temp == -127.0) continue; 

      //===============================================
      // --- State Machine Logic for Sensor [i] ---
      //===============================================

      // --- 1. HEATING/HOLDING LOGIC (Output Control) ---
      // This logic controls the physical output pin (heater).
      
      // Condition: Turn ON output
      // If temp is at or above the setpoint, turn on the output.
      if (temp >= thresholdTemps[i] && !outputState[i]) {
        digitalWrite(outputPins[i], HIGH);
        outputState[i] = true;
        Serial.println("Sensor " + String(i) + ": Temp above threshold. Output ON.");
        
        // --- State Transition: IDLE -> HOLD ---
        // If we just crossed the threshold and are not already in a phase,
        // start the HOLD phase.
        if (!holdPhaseActive[i] && !coolingPhaseActive[i]) {
            holdPhaseActive[i] = true;
            phaseStartMillis[i] = currentMillis; // Start the hold timer
            Serial.println("Sensor " + String(i) + ": Hold phase started.");
        }

      // Condition: Turn OFF output (with Hysteresis)
      // If temp drops below the setpoint *minus* the hysteresis value,
      // turn off the output.
      } else if (temp < (thresholdTemps[i] - HYSTERESIS) && outputState[i]) {
        digitalWrite(outputPins[i], LOW);
        outputState[i] = false;
        Serial.println("Sensor " + String(i) + ": Temp below threshold-hysteresis. Output OFF.");
      }

      // --- 2. HOLD PHASE LOGIC ---
      // This logic checks if the hold timer has expired.
      if (holdPhaseActive[i]) {
        unsigned long holdDurationMillis = holdDurations[i] * 60 * 1000;
        
        // Check if the elapsed time has exceeded the desired hold duration
        if (currentMillis - phaseStartMillis[i] >= holdDurationMillis) {
          
          // --- State Transition: HOLD -> COOLING ---
          holdPhaseActive[i] = false;
          coolingPhaseActive[i] = true;
          phaseStartMillis[i] = currentMillis; // Reset timer for cooling phase
          Serial.println("Sensor " + String(i) + ": Hold phase finished. Cooling phase started.");
        }
      }

      // --- 3. COOLING RAMP LOGIC ---
      // This logic dynamically lowers the 'thresholdTemps' setpoint over time.
      if (coolingPhaseActive[i]) {
        
        // Only ramp down if the current setpoint is still above the floor
        if (thresholdTemps[i] > lowerLimits[i]) {
          
          // Calculate how much the setpoint should decrease in this time slice
          float degreesPerMilli = coolingSpeeds[i] / 60000.0;
          float decreaseAmount = degreesPerMilli * elapsedSinceUpdate;
          
          // Apply the decrease to the "live" setpoint
          thresholdTemps[i] -= decreaseAmount;

          // Clamp the setpoint to the lower limit to prevent overshooting
          if (thresholdTemps[i] < lowerLimits[i]) {
            thresholdTemps[i] = lowerLimits[i];
          }
          
        } else {
          // --- State Transition: COOLING -> IDLE ---
          // The cooling ramp is complete.
          coolingPhaseActive[i] = false;
          Serial.println("Sensor " + String(i) + ": Cooling finished. Reached lower limit.");
        }
      }
    }
  }
}
