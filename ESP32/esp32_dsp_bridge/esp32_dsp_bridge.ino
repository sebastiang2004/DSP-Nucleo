/**
 * ESP32 DSP Bridge - Connects STM32 DSP to Node.js Backend via WiFi
 * 
 * This sketch allows the ESP32 to:
 * 1. Communicate with STM32 via UART (Serial2)
 * 2. Connect to a backend Node.js API and forward commands
 * 3. Act as a WiFi bridge between Flutter app -> Backend -> ESP32 -> STM32
 * 
 * Hardware Connection:
 * ESP32 TX (GPIO17) -> STM32 RX (PA3)
 * ESP32 RX (GPIO16) -> STM32 TX (PA2)
 * ESP32 GND -> STM32 GND
 * 
 * IMPORTANT: STM32 operates at 3.3V, ESP32 operates at 3.3V
 * Direct connection is safe (both are 3.3V logic)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* ssid = "iPhone";           // Change this
const char* password = "pfku67812";    // Change this

// Backend API Configuration
const char* backend_url = "http://192.168.1.100:3000";  // Change to your backend IP
const int poll_interval_ms = 500;  // Poll backend every 500ms for updates

// UART to STM32 (Serial2)
// Using USART3 on STM32 (PC10/PC11) instead of USART2 to avoid ST-LINK conflict
#define STM32_RX_PIN 16  // ESP32 RX <- STM32 TX (PC10 - USART3_TX)
#define STM32_TX_PIN 17  // ESP32 TX -> STM32 RX (PC11 - USART3_RX)
#define STM32_BAUD 115200

// Effect parameters (updated for optimized STM32 effects)
struct EffectParams {
  float volume;
  bool overdrive_enabled;
  float overdrive_gain;        // 1.0 - 100.0 (expanded range)
  float overdrive_threshold;   // 0.1 - 0.95
  float overdrive_tone;        // 0.0 - 1.0 (now functional!)
  float overdrive_mix;         // 0.0 - 1.0 (wet/dry mix)
  int overdrive_mode;          // 0=soft, 1=hard, 2=asymmetric
  bool delay_enabled;
  float delay_time_ms;
  float delay_feedback;
  float delay_mix;
  float delay_tone;            // NEW: 0.0 - 1.0 (damping filter)
  bool gate_enabled;           // NEW: Noise gate
  float gate_threshold;        // 0.001 - 0.5
  float gate_attack;           // 0.0001 - 0.1
  float gate_release;          // 0.01 - 1.0
};

EffectParams effects = {
  .volume = 0.7,               // Safe default volume
  .overdrive_enabled = false,  // *** DISABLED by default - clean signal on startup ***
  .overdrive_gain = 5.0,       // Low gain when enabled (was 20!)
  .overdrive_threshold = 0.7,
  .overdrive_tone = 0.5,
  .overdrive_mix = 0.5,        // 50/50 wet/dry blend
  .overdrive_mode = 0,         // Soft clipping (smoother)
  .delay_enabled = false,      // Disabled by default
  .delay_time_ms = 100.0,
  .delay_feedback = 0.3,
  .delay_mix = 0.3,
  .delay_tone = 0.5,           // Mid tone setting
  .gate_enabled = true,        // Gate enabled to reduce noise
  .gate_threshold = 0.015,     // Lower threshold (less aggressive)
  .gate_attack = 0.001,
  .gate_release = 0.15
};

// Function prototypes
void sendToSTM32(String command);
String receiveFromSTM32(int timeout_ms = 500);
bool sendCommandAndWaitForAck(String command, int timeout_ms = 1000);
void pollBackendForUpdates();
void applyEffectsFromJson(JsonObject effects);
void reconnectWiFi();
bool waitForSTM32Ready(uint32_t timeout_ms = 5000);

void setup() {
  // Start Serial for debugging
  Serial.begin(115200);
  Serial.println("\n\nESP32 DSP Bridge Starting...");
  
  // Start Serial2 for STM32 communication
  Serial2.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  Serial.println("UART to STM32 initialized");
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Backend URL: ");
    Serial.println(backend_url);
    Serial.println("Ready to receive commands from backend...");
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("ESP32 will not be able to communicate with backend.");
  }
  
  delay(1000);
  
  Serial.println("\n=== Initializing STM32 Communication ===");
  
  // Clear UART buffers
  while (Serial2.available()) {
    Serial2.read();
  }
  
  // Wait for STM32 to be ready
  Serial.println("Waiting for STM32 to be ready...");
  waitForSTM32Ready(5000);
  delay(200);
  
  // Initialize STM32 with default settings (with validation)
  Serial.println("Setting volume...");
  if (!sendCommandAndWaitForAck("VOL:" + String(effects.volume, 2), 800)) {
    Serial.println("WARNING: Volume command not acknowledged");
  }
  delay(100);
  
  Serial.println("Setting overdrive parameters...");
  if (!sendCommandAndWaitForAck("OVR:" + String(effects.overdrive_gain, 1) + "," + 
                                String(effects.overdrive_threshold, 2) + "," + 
                                String(effects.overdrive_tone, 2) + "," +
                                String(effects.overdrive_mix, 2) + "," +
                                String(effects.overdrive_mode), 800)) {
    Serial.println("WARNING: Overdrive parameters not acknowledged");
  }
  delay(100);
  
  Serial.println("Enabling overdrive...");
  if (!sendCommandAndWaitForAck(effects.overdrive_enabled ? "OVR:ON" : "OVR:OFF", 800)) {
    Serial.println("WARNING: Overdrive toggle not acknowledged");
  }
  delay(100);
  
  Serial.println("Setting delay parameters...");
  if (!sendCommandAndWaitForAck("DLY:" + String(effects.delay_time_ms, 0) + "," + 
                                String(effects.delay_feedback, 2) + "," + 
                                String(effects.delay_mix, 2) + "," +
                                String(effects.delay_tone, 2), 800)) {
    Serial.println("WARNING: Delay parameters not acknowledged");
  }
  delay(100);
  
  Serial.println("Setting noise gate...");
  if (!sendCommandAndWaitForAck("GATE:" + String(effects.gate_threshold, 3) + "," + 
                                String(effects.gate_attack, 4) + "," + 
                                String(effects.gate_release, 2), 800)) {
    Serial.println("WARNING: Gate parameters not acknowledged");
  }
  delay(100);
  
  if (!sendCommandAndWaitForAck(effects.gate_enabled ? "GATE:ON" : "GATE:OFF", 800)) {
    Serial.println("WARNING: Gate toggle not acknowledged");
  }
  delay(100);
  
  Serial.println("\n=== Setup complete! ===");
  Serial.println("ESP32 is ready to bridge commands from backend to STM32");
  Serial.println("Waiting for commands from: " + String(backend_url));
  Serial.println();
}

void loop() {
  // Check WiFi connection and reconnect if needed
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 5000) {  // Check every 5 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Attempting to reconnect...");
      reconnectWiFi();
    }
    lastWiFiCheck = millis();
  }
  
  // Poll backend for effect updates
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll > poll_interval_ms) {
    if (WiFi.status() == WL_CONNECTED) {
      pollBackendForUpdates();
    }
    lastPoll = millis();
  }
  
  // Listen for ACK messages from STM32 (don't spam Serial Monitor)
  static String stm32_buffer = "";
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      // Complete message received - only print ACK messages, not debug spam
      if (stm32_buffer.length() > 0 && stm32_buffer.startsWith("ACK:")) {
        Serial.print("<- STM32: ");
        Serial.println(stm32_buffer);
      }
      stm32_buffer = "";  // Clear buffer
    } else if (c >= 32 && c <= 126) {  // Printable ASCII
      stm32_buffer += c;
    }
  }
}

/**
 * Send command to STM32 via UART
 */
void sendToSTM32(String command) {
  // Clear any pending data before sending
  while (Serial2.available()) {
    Serial2.read();
  }
  
  // Send command (no serial spam)
  Serial2.print(command);
  Serial2.print('\n');
  Serial2.flush();
  
  delay(10);  // Small delay to ensure transmission
}

/**
 * Receive response from STM32 with timeout
 */
String receiveFromSTM32(int timeout_ms) {
  String response = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < timeout_ms) {
    if (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\n' || c == '\r') {
        if (response.length() > 0) {
          break;
        }
      } else if (c >= 32 && c <= 126) {
        response += c;
      }
    }
    delay(1);
  }
  
  return response;
}

/**
 * Send command and wait for ACK from STM32
 * Returns true if ACK received, false if timeout
 */

bool waitForSTM32Ready(uint32_t timeout_ms) {
  unsigned long start = millis();
  String line = "";

  while (millis() - start < timeout_ms) {
    while (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\n' || c == '\r') {
        if (line.length() > 0) {
          if (line.equals("STM32_READY")) {
            Serial.println("STM32 ready!");
            return true;
          }
          // Ignore other startup messages
          line = "";
        }
      } else if (c >= 32 && c <= 126) {
        line += c;
      }
    }
    delay(5);
  }

  Serial.println("WARNING: STM32 READY message not received (timeout)");
  return false;
}
bool sendCommandAndWaitForAck(String command, int timeout_ms) {
  // Clear buffer
  while (Serial2.available()) {
    Serial2.read();
  }
  
  // Send command
  Serial2.print(command);
  Serial2.print('\n');
  Serial2.flush();
  
  // Wait for ACK response
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < timeout_ms) {
    if (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\n' || c == '\r') {
        if (response.length() > 0) {
          // Check if response starts with "ACK:"
          if (response.startsWith("ACK:")) {
            return true;  // Success - no need to print every ACK
          }
        }
        response = "";  // Reset for next line
      } else if (c >= 32 && c <= 126) {
        response += c;
      }
    }
    // Don't use delay(1) - wastes time. Just yield()
    yield();
  }
  
  // Only print errors to reduce serial spam
  Serial.print("✗ ACK timeout for: ");
  Serial.println(command);
  return false;
}

/**
 * Poll backend for effect updates and apply them to STM32
 */
void pollBackendForUpdates() {
  HTTPClient http;
  http.begin(String(backend_url) + "/api/effects");
  http.setTimeout(2000);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && doc.containsKey("effects")) {
      JsonObject effectsObj = doc["effects"].as<JsonObject>();
      applyEffectsFromJson(effectsObj);
    }
  } else if (httpCode > 0) {
    Serial.print("Backend HTTP error: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

/**
 * Apply effects from JSON received from backend
 */
void applyEffectsFromJson(JsonObject json) {
  static EffectParams lastEffects = effects;  // Track changes
  bool changed = false;
  
  // Check volume
  if (json.containsKey("volume")) {
    float newVol = json["volume"];
    if (abs(newVol - lastEffects.volume) > 0.01) {
      effects.volume = newVol;
      sendCommandAndWaitForAck("VOL:" + String(effects.volume, 2), 1000);
      Serial.println("-> Volume: " + String(effects.volume * 100) + "%");
      lastEffects.volume = effects.volume;
      changed = true;
    }
  }
  
  // Check overdrive
  if (json.containsKey("overdrive")) {
    JsonObject ovr = json["overdrive"];
    bool paramsChanged = false;
    
    if (ovr.containsKey("gain") && abs(ovr["gain"].as<float>() - lastEffects.overdrive_gain) > 0.1) {
      effects.overdrive_gain = ovr["gain"];
      paramsChanged = true;
    }
    if (ovr.containsKey("threshold") && abs(ovr["threshold"].as<float>() - lastEffects.overdrive_threshold) > 0.01) {
      effects.overdrive_threshold = ovr["threshold"];
      paramsChanged = true;
    }
    if (ovr.containsKey("tone") && abs(ovr["tone"].as<float>() - lastEffects.overdrive_tone) > 0.01) {
      effects.overdrive_tone = ovr["tone"];
      paramsChanged = true;
    }
    if (ovr.containsKey("mix") && abs(ovr["mix"].as<float>() - lastEffects.overdrive_mix) > 0.01) {
      effects.overdrive_mix = ovr["mix"];
      paramsChanged = true;
    }
    if (ovr.containsKey("mode") && ovr["mode"].as<int>() != lastEffects.overdrive_mode) {
      effects.overdrive_mode = ovr["mode"];
      paramsChanged = true;
    }
    
    if (paramsChanged && effects.overdrive_enabled) {
      String cmd = "OVR:" + String(effects.overdrive_gain, 1) + "," + 
                   String(effects.overdrive_threshold, 2) + "," + 
                   String(effects.overdrive_tone, 2) + "," +
                   String(effects.overdrive_mix, 2) + "," +
                   String(effects.overdrive_mode);
      sendCommandAndWaitForAck(cmd, 1500);
      Serial.println("-> Overdrive params updated");
      lastEffects.overdrive_gain = effects.overdrive_gain;
      lastEffects.overdrive_threshold = effects.overdrive_threshold;
      lastEffects.overdrive_tone = effects.overdrive_tone;
      lastEffects.overdrive_mix = effects.overdrive_mix;
      lastEffects.overdrive_mode = effects.overdrive_mode;
      changed = true;
      delay(100);
    }
    
    if (ovr.containsKey("enabled") && ovr["enabled"].as<bool>() != lastEffects.overdrive_enabled) {
      effects.overdrive_enabled = ovr["enabled"];
      sendCommandAndWaitForAck(effects.overdrive_enabled ? "OVR:ON" : "OVR:OFF", 1000);
      Serial.println("-> Overdrive " + String(effects.overdrive_enabled ? "ON" : "OFF"));
      lastEffects.overdrive_enabled = effects.overdrive_enabled;
      changed = true;
    }
  }
  
  // Check delay
  if (json.containsKey("delay")) {
    JsonObject dly = json["delay"];
    bool paramsChanged = false;
    
    if (dly.containsKey("time_ms") && abs(dly["time_ms"].as<float>() - lastEffects.delay_time_ms) > 1.0) {
      effects.delay_time_ms = dly["time_ms"];
      paramsChanged = true;
    }
    if (dly.containsKey("feedback") && abs(dly["feedback"].as<float>() - lastEffects.delay_feedback) > 0.01) {
      effects.delay_feedback = dly["feedback"];
      paramsChanged = true;
    }
    if (dly.containsKey("mix") && abs(dly["mix"].as<float>() - lastEffects.delay_mix) > 0.01) {
      effects.delay_mix = dly["mix"];
      paramsChanged = true;
    }
    if (dly.containsKey("tone") && abs(dly["tone"].as<float>() - lastEffects.delay_tone) > 0.01) {
      effects.delay_tone = dly["tone"];
      paramsChanged = true;
    }
    
    if (paramsChanged && effects.delay_enabled) {
      String cmd = "DLY:" + String(effects.delay_time_ms, 0) + "," + 
                   String(effects.delay_feedback, 2) + "," + 
                   String(effects.delay_mix, 2) + "," +
                   String(effects.delay_tone, 2);
      sendCommandAndWaitForAck(cmd, 1500);
      Serial.println("-> Delay params updated");
      lastEffects.delay_time_ms = effects.delay_time_ms;
      lastEffects.delay_feedback = effects.delay_feedback;
      lastEffects.delay_mix = effects.delay_mix;
      lastEffects.delay_tone = effects.delay_tone;
      changed = true;
      delay(100);
    }
    
    if (dly.containsKey("enabled") && dly["enabled"].as<bool>() != lastEffects.delay_enabled) {
      effects.delay_enabled = dly["enabled"];
      sendCommandAndWaitForAck(effects.delay_enabled ? "DLY:ON" : "DLY:OFF", 1000);
      Serial.println("-> Delay " + String(effects.delay_enabled ? "ON" : "OFF"));
      lastEffects.delay_enabled = effects.delay_enabled;
      changed = true;
    }
  }
  
  // Check noise gate
  if (json.containsKey("gate")) {
    JsonObject gate = json["gate"];
    bool paramsChanged = false;
    
    if (gate.containsKey("threshold") && abs(gate["threshold"].as<float>() - lastEffects.gate_threshold) > 0.001) {
      effects.gate_threshold = gate["threshold"];
      paramsChanged = true;
    }
    if (gate.containsKey("attack") && abs(gate["attack"].as<float>() - lastEffects.gate_attack) > 0.0001) {
      effects.gate_attack = gate["attack"];
      paramsChanged = true;
    }
    if (gate.containsKey("release") && abs(gate["release"].as<float>() - lastEffects.gate_release) > 0.01) {
      effects.gate_release = gate["release"];
      paramsChanged = true;
    }
    
    if (paramsChanged && effects.gate_enabled) {
      String cmd = "GATE:" + String(effects.gate_threshold, 3) + "," + 
                   String(effects.gate_attack, 4) + "," + 
                   String(effects.gate_release, 2);
      sendCommandAndWaitForAck(cmd, 1500);
      Serial.println("-> Gate params updated");
      lastEffects.gate_threshold = effects.gate_threshold;
      lastEffects.gate_attack = effects.gate_attack;
      lastEffects.gate_release = effects.gate_release;
      changed = true;
      delay(100);
    }
    
    if (gate.containsKey("enabled") && gate["enabled"].as<bool>() != lastEffects.gate_enabled) {
      effects.gate_enabled = gate["enabled"];
      sendCommandAndWaitForAck(effects.gate_enabled ? "GATE:ON" : "GATE:OFF", 1000);
      Serial.println("-> Gate " + String(effects.gate_enabled ? "ON" : "OFF"));
      lastEffects.gate_enabled = effects.gate_enabled;
      changed = true;
    }
  }
  
  if (changed) {
    Serial.println("✓ Effects synced from backend");
  }
}

/**
 * Reconnect to WiFi
 */
void reconnectWiFi() {
  Serial.println("Reconnecting to WiFi...");
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi reconnection failed");
  }
}


