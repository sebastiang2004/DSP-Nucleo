/**
 * ESP32 DSP Bridge - Connects STM32 DSP to WiFi/Web API
 * 
 * This sketch allows the ESP32 to:
 * 1. Communicate with STM32 via UART (Serial2)
 * 2. Host a WiFi web server for DSP control
 * 3. Connect to a backend Node.js API
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
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* ssid = "Orange-F80C_RPT";           // Change this
const char* password = "HtC75TuGUyQY7TFdK5";    // Change this

// Backend API Configuration (optional)
const char* backend_url = "http://192.168.1.100:3000/api/effects";  // Change to your backend IP

// Web Server
WebServer server(80);

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
void handleRoot();
void handleSetVolume();
void handleSetOverdrive();
void handleSetDelay();
void handleSetGate();
void handleGetStatus();
void handleNotFound();
void syncWithBackend();
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
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/api/volume", HTTP_POST, handleSetVolume);
    server.on("/api/overdrive", HTTP_POST, handleSetOverdrive);
    server.on("/api/delay", HTTP_POST, handleSetDelay);
    server.on("/api/gate", HTTP_POST, handleSetGate);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/preset/clean", HTTP_POST, []() { handleLoadPresetByName("clean"); });
    server.on("/api/preset/crunch", HTTP_POST, []() { handleLoadPresetByName("crunch"); });
    server.on("/api/preset/lead", HTTP_POST, []() { handleLoadPresetByName("lead"); });
    server.on("/api/preset/metal", HTTP_POST, []() { handleLoadPresetByName("metal"); });
    server.on("/api/preset/ambient", HTTP_POST, []() { handleLoadPresetByName("ambient"); });
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("Web server started");
  } else {
    Serial.println("\nWiFi connection failed - running in standalone mode");
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
  Serial.println("Access the web interface at: http://" + WiFi.localIP().toString());
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
  
  // Handle web server requests
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
  
  // Listen for ACK messages from STM32 (don't spam Serial Monitor)
  static String stm32_buffer = "";
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      // Complete message received - only print ACK messages, not debug spam
      if (stm32_buffer.length() > 0 && stm32_buffer.startsWith("ACK:")) {
        Serial.print("<- ");
        Serial.println(stm32_buffer);
      }
      stm32_buffer = "";  // Clear buffer
    } else if (c >= 32 && c <= 126) {  // Printable ASCII
      stm32_buffer += c;
    }
  }

  // Removed backend sync - was causing lag and probably not needed
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
 * Web Server Handlers
 */

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>STM32 DSP Control</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Arial; margin: 0; padding: 20px; background: linear-gradient(135deg, #1a1a1a 0%, #2d2d2d 100%); color: #fff; }";
  html += "h1 { color: #4CAF50; text-align: center; margin-bottom: 10px; }";
  html += ".info { text-align: center; color: #888; margin-bottom: 30px; font-size: 14px; }";
  html += ".presets { display: flex; flex-wrap: wrap; gap: 10px; justify-content: center; margin-bottom: 30px; }";
  html += ".preset-btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 12px 24px; border: none; border-radius: 25px; cursor: pointer; font-size: 14px; font-weight: bold; transition: all 0.3s; box-shadow: 0 4px 15px rgba(0,0,0,0.3); }";
  html += ".preset-btn:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(102, 126, 234, 0.4); }";
  html += ".preset-btn:active { transform: translateY(0); }";
  html += ".control { margin: 20px auto; max-width: 600px; padding: 20px; background: rgba(42, 42, 42, 0.9); border-radius: 15px; box-shadow: 0 8px 32px rgba(0,0,0,0.3); backdrop-filter: blur(10px); }";
  html += ".control h2 { margin-top: 0; color: #4CAF50; font-size: 20px; display: flex; align-items: center; justify-content: space-between; }";
  html += ".toggle-btn { background: #f44336; padding: 8px 20px; border-radius: 20px; font-size: 14px; min-width: 100px; }";
  html += ".toggle-btn.active { background: #4CAF50; }";
  html += ".slider-container { margin: 15px 0; }";
  html += ".slider-label { display: flex; justify-content: space-between; margin-bottom: 8px; font-size: 14px; color: #bbb; }";
  html += "input[type='range'] { width: 100%; height: 8px; border-radius: 5px; background: #444; outline: none; -webkit-appearance: none; }";
  html += "input[type='range']::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 20px; height: 20px; border-radius: 50%; background: #4CAF50; cursor: pointer; box-shadow: 0 0 10px rgba(76, 175, 80, 0.5); }";
  html += "input[type='range']::-moz-range-thumb { width: 20px; height: 20px; border-radius: 50%; background: #4CAF50; cursor: pointer; border: none; }";
  html += ".value { font-weight: bold; color: #4CAF50; font-size: 16px; }";
  html += ".status { position: fixed; bottom: 20px; right: 20px; background: rgba(76, 175, 80, 0.9); padding: 10px 20px; border-radius: 25px; font-size: 12px; display: none; animation: slideIn 0.3s; }";
  html += "@keyframes slideIn { from { transform: translateY(100px); opacity: 0; } to { transform: translateY(0); opacity: 1; } }";
  html += ".icon { margin-right: 8px; }";
  html += "</style>";
  html += "</head><body>";
  
  html += "<h1>🎸 STM32 DSP Guitar Pedal</h1>";
  html += "<div class='info'>ESP32 IP: " + WiFi.localIP().toString() + " | Status: <span style='color:#4CAF50'>● Connected</span></div>";
  
  // Presets Section
  html += "<div class='presets'>";
  html += "<button class='preset-btn' onclick='loadPreset(\"clean\")'>Clean</button>";
  html += "<button class='preset-btn' onclick='loadPreset(\"crunch\")'>Crunch</button>";
  html += "<button class='preset-btn' onclick='loadPreset(\"lead\")'>Lead</button>";
  html += "<button class='preset-btn' onclick='loadPreset(\"metal\")'>Metal</button>";
  html += "<button class='preset-btn' onclick='loadPreset(\"ambient\")'>Ambient</button>";
  html += "</div>";
  
  // Volume Control
  html += "<div class='control'>";
  html += "<h2><span class='icon'>V</span>Master Volume</h2>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Level</span><span class='value' id='vol-val'>" + String((int)(effects.volume * 100)) + "%</span></div>";
  html += "<input type='range' min='0' max='100' value='" + String((int)(effects.volume * 100)) + "' id='volume' oninput='updateValue(\"vol\", this.value, \"%\");' onchange='applyVolume(this.value);'>";
  html += "</div></div>";
  
  // Overdrive Control
  html += "<div class='control'>";
  html += "<h2><span class='icon'>C</span>Overdrive<button class='toggle-btn " + String(effects.overdrive_enabled ? "active" : "") + "' id='ovr-toggle' onclick='toggleEffect(\"overdrive\")'>" + String(effects.overdrive_enabled ? "ON" : "OFF") + "</button></h2>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Gain</span><span class='value' id='ovr-gain-val'>" + String(effects.overdrive_gain, 1) + "</span></div>";
  html += "<input type='range' min='10' max='300' value='" + String((int)(effects.overdrive_gain * 10)) + "' id='ovr-gain' oninput='updateValue(\"ovr-gain\", (this.value/10).toFixed(1), \"\");' onchange='applyOverdrive();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Threshold</span><span class='value' id='ovr-thresh-val'>" + String(effects.overdrive_threshold, 2) + "</span></div>";
  html += "<input type='range' min='10' max='95' value='" + String((int)(effects.overdrive_threshold * 100)) + "' id='ovr-thresh' oninput='updateValue(\"ovr-thresh\", (this.value/100).toFixed(2), \"\");' onchange='applyOverdrive();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Tone</span><span class='value' id='ovr-tone-val'>" + String(effects.overdrive_tone, 2) + "</span></div>";
  html += "<input type='range' min='0' max='100' value='" + String((int)(effects.overdrive_tone * 100)) + "' id='ovr-tone' oninput='updateValue(\"ovr-tone\", (this.value/100).toFixed(2), \"\");' onchange='applyOverdrive();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Mix (Wet/Dry)</span><span class='value' id='ovr-mix-val'>" + String((int)(effects.overdrive_mix * 100)) + "%</span></div>";
  html += "<input type='range' min='0' max='100' value='" + String((int)(effects.overdrive_mix * 100)) + "' id='ovr-mix' oninput='updateValue(\"ovr-mix\", this.value, \"%\");' onchange='applyOverdrive();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Mode</span><span class='value' id='ovr-mode-val'>" + String(effects.overdrive_mode == 0 ? "Soft" : (effects.overdrive_mode == 1 ? "Hard" : "Asymmetric")) + "</span></div>";
  html += "<select id='ovr-mode' onchange='updateModeValue(this.value); applyOverdrive();' style='width:100%; padding:10px; background:#444; color:#fff; border:1px solid #666; border-radius:8px;'>";
  html += "<option value='0'" + String(effects.overdrive_mode == 0 ? " selected" : "") + ">Soft (Tube-like)</option>";
  html += "<option value='1'" + String(effects.overdrive_mode == 1 ? " selected" : "") + ">Hard (Solid-state)</option>";
  html += "<option value='2'" + String(effects.overdrive_mode == 2 ? " selected" : "") + ">Asymmetric (Diode)</option>";
  html += "</select></div></div>";
  
  // Delay Control
  html += "<div class='control'>";
  html += "<h2><span class='icon'>DC</span>Delay<button class='toggle-btn " + String(effects.delay_enabled ? "active" : "") + "' id='dly-toggle' onclick='toggleEffect(\"delay\")'>" + String(effects.delay_enabled ? "ON" : "OFF") + "</button></h2>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Time</span><span class='value' id='dly-time-val'>" + String((int)effects.delay_time_ms) + "ms</span></div>";
  html += "<input type='range' min='20' max='100' value='" + String((int)effects.delay_time_ms) + "' id='dly-time' oninput='updateValue(\"dly-time\", this.value, \"ms\");' onchange='applyDelay();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Feedback</span><span class='value' id='dly-fb-val'>" + String((int)(effects.delay_feedback * 100)) + "%</span></div>";
  html += "<input type='range' min='0' max='95' value='" + String((int)(effects.delay_feedback * 100)) + "' id='dly-fb' oninput='updateValue(\"dly-fb\", this.value, \"%\");' onchange='applyDelay();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Mix</span><span class='value' id='dly-mix-val'>" + String((int)(effects.delay_mix * 100)) + "%</span></div>";
  html += "<input type='range' min='0' max='100' value='" + String((int)(effects.delay_mix * 100)) + "' id='dly-mix' oninput='updateValue(\"dly-mix\", this.value, \"%\");' onchange='applyDelay();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Tone (Damping)</span><span class='value' id='dly-tone-val'>" + String((int)(effects.delay_tone * 100)) + "%</span></div>";
  html += "<input type='range' min='0' max='100' value='" + String((int)(effects.delay_tone * 100)) + "' id='dly-tone' oninput='updateValue(\"dly-tone\", this.value, \"%\");' onchange='applyDelay();'>";
  html += "</div></div>";
  
  // Noise Gate Control (NEW!)
  html += "<div class='control'>";
  html += "<h2><span class='icon'>🚪</span>Noise Gate<button class='toggle-btn " + String(effects.gate_enabled ? "active" : "") + "' id='gate-toggle' onclick='toggleEffect(\"gate\")'>" + String(effects.gate_enabled ? "ON" : "OFF") + "</button></h2>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Threshold</span><span class='value' id='gate-thresh-val'>" + String(effects.gate_threshold, 3) + "</span></div>";
  html += "<input type='range' min='1' max='500' value='" + String((int)(effects.gate_threshold * 1000)) + "' id='gate-thresh' oninput='updateValue(\"gate-thresh\", (this.value/1000).toFixed(3), \"\");' onchange='applyGate();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Attack (ms)</span><span class='value' id='gate-attack-val'>" + String(effects.gate_attack * 1000, 1) + "ms</span></div>";
  html += "<input type='range' min='1' max='100' value='" + String((int)(effects.gate_attack * 10000)) + "' id='gate-attack' oninput='updateValue(\"gate-attack\", (this.value/10).toFixed(1), \"ms\");' onchange='applyGate();'>";
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Release (ms)</span><span class='value' id='gate-release-val'>" + String((int)(effects.gate_release * 1000)) + "ms</span></div>";
  html += "<input type='range' min='10' max='1000' value='" + String((int)(effects.gate_release * 1000)) + "' id='gate-release' oninput='updateValue(\"gate-release\", this.value, \"ms\");' onchange='applyGate();'>";
  html += "</div></div>";
  
  html += "<div class='status' id='status'>✓ Updated</div>";
  
  // JavaScript
  html += "<script>";
  html += "function updateValue(id, value, unit) { document.getElementById(id + '-val').innerText = value + unit; }";
  html += "function updateModeValue(value) { const modes = ['Soft', 'Hard', 'Asymmetric']; document.getElementById('ovr-mode-val').innerText = modes[value]; }";
  html += "function showStatus(msg) { const s = document.getElementById('status'); s.innerText = msg; s.style.display = 'block'; setTimeout(() => s.style.display = 'none', 2000); }";
  
  html += "function applyVolume(val) { fetch('/api/volume', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({volume:val/100})}).then(r=>r.json()).then(d=>showStatus('✓ Volume updated')).catch(e=>showStatus('✗ Error')); }";
  
  html += "function applyOverdrive() { ";
  html += "const gain = document.getElementById('ovr-gain').value / 10; ";
  html += "const threshold = document.getElementById('ovr-thresh').value / 100; ";
  html += "const tone = document.getElementById('ovr-tone').value / 100; ";
  html += "const mix = document.getElementById('ovr-mix').value / 100; ";
  html += "const mode = document.getElementById('ovr-mode').value; ";
  html += "fetch('/api/overdrive', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({gain:gain, threshold:threshold, tone:tone, mix:mix, mode:parseInt(mode)})}).then(r=>r.json()).then(d=>showStatus('✓ Overdrive updated')).catch(e=>showStatus('✗ Error')); }";
  
  html += "function applyDelay() { ";
  html += "const time = document.getElementById('dly-time').value; ";
  html += "const feedback = document.getElementById('dly-fb').value / 100; ";
  html += "const mix = document.getElementById('dly-mix').value / 100; ";
  html += "const tone = document.getElementById('dly-tone').value / 100; ";
  html += "fetch('/api/delay', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({time:time, feedback:feedback, mix:mix, tone:tone})}).then(r=>r.json()).then(d=>showStatus('✓ Delay updated')).catch(e=>showStatus('✗ Error')); }";
  
  html += "function applyGate() { ";
  html += "const threshold = document.getElementById('gate-thresh').value / 1000; ";
  html += "const attack = document.getElementById('gate-attack').value / 10000; ";
  html += "const release = document.getElementById('gate-release').value / 1000; ";
  html += "fetch('/api/gate', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({threshold:threshold, attack:attack, release:release})}).then(r=>r.json()).then(d=>showStatus('✓ Gate updated')).catch(e=>showStatus('✗ Error')); }";
  
  html += "function toggleEffect(effect) { ";
  html += "const btnMap = {'overdrive':'ovr-toggle', 'delay':'dly-toggle', 'gate':'gate-toggle'}; ";
  html += "const endpointMap = {'overdrive':'/api/overdrive', 'delay':'/api/delay', 'gate':'/api/gate'}; ";
  html += "const btn = document.getElementById(btnMap[effect]); ";
  html += "const isActive = btn.classList.contains('active'); ";
  html += "fetch(endpointMap[effect], {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({enabled:!isActive})}).then(r=>r.json()).then(d=>{ btn.classList.toggle('active'); btn.innerText = isActive ? 'OFF' : 'ON'; showStatus('✓ ' + effect + ' ' + (isActive ? 'disabled' : 'enabled')); }).catch(e=>showStatus('✗ Error')); }";
  
  html += "function loadPreset(name) { fetch('/api/preset/' + name, {method:'POST'}).then(r=>r.json()).then(d=>{ if(d.success) { showStatus('✓ Preset \"' + name + '\" loaded'); setTimeout(()=>location.reload(), 1000); } }).catch(e=>showStatus('✗ Error loading preset')); }";
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSetVolume() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg("plain"));
    
    float volume = doc["volume"];
    if (volume >= 0.0 && volume <= 1.0) {
      effects.volume = volume;
      
      String cmd = "VOL:" + String(volume, 2);
      bool success = sendCommandAndWaitForAck(cmd, 1000);
      
      if (success) {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Volume set to " + String(volume * 100) + "%\"}");
      } else {
        server.send(500, "application/json", "{\"success\":false,\"message\":\"STM32 did not acknowledge command\"}");
      }
    } else {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid volume value\"}");
    }
  }
}

void handleSetOverdrive() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<300> doc;
    deserializeJson(doc, server.arg("plain"));
    
    bool success = true;
    String message = "Overdrive updated";
    bool hasToggle = doc.containsKey("enabled");
    bool hasParams = doc.containsKey("gain") || doc.containsKey("threshold") || 
                     doc.containsKey("tone") || doc.containsKey("mix") || doc.containsKey("mode");
    
    // Update local effect state
    if (doc.containsKey("enabled")) {
      effects.overdrive_enabled = doc["enabled"];
    }
    if (doc.containsKey("gain")) {
      effects.overdrive_gain = doc["gain"];
    }
    if (doc.containsKey("threshold")) {
      effects.overdrive_threshold = doc["threshold"];
    }
    if (doc.containsKey("tone")) {
      effects.overdrive_tone = doc["tone"];
    }
    if (doc.containsKey("mix")) {
      effects.overdrive_mix = doc["mix"];
    }
    if (doc.containsKey("mode")) {
      effects.overdrive_mode = doc["mode"];
    }
    
    // Send commands in the right order
    // 1. If parameters changed AND effect is enabled, send parameters first
    if (hasParams && effects.overdrive_enabled) {
      String cmd = "OVR:" + String(effects.overdrive_gain, 1) + "," + 
                   String(effects.overdrive_threshold, 2) + "," + 
                   String(effects.overdrive_tone, 2) + "," +
                   String(effects.overdrive_mix, 2) + "," +
                   String(effects.overdrive_mode);
      
      if (!sendCommandAndWaitForAck(cmd, 2000)) {
        success = false;
        message = "Failed to set overdrive parameters";
      }
      delay(100);
    }
    
    // 2. Then send toggle command if state changed
    if (hasToggle) {
      String cmd = effects.overdrive_enabled ? "OVR:ON" : "OVR:OFF";
      if (!sendCommandAndWaitForAck(cmd, 2000)) {
        success = false;
        message = "Failed to toggle overdrive";
      }
    }
    
    // 3. If parameters changed while effect is disabled, just update them (don't send to STM32)
    //    They'll be applied when effect is enabled again
    
    if (success) {
      server.send(200, "application/json", "{\"success\":true,\"message\":\"" + message + "\"}");
    } else {
      server.send(500, "application/json", "{\"success\":false,\"message\":\"" + message + "\"}");
    }
  }
}

void handleSetDelay() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<300> doc;
    deserializeJson(doc, server.arg("plain"));
    
    bool success = true;
    String message = "Delay updated";
    bool hasToggle = doc.containsKey("enabled");
    bool hasParams = doc.containsKey("time") || doc.containsKey("mix") || 
                     doc.containsKey("feedback") || doc.containsKey("tone");
    
    // Update local effect state
    if (doc.containsKey("enabled")) {
      effects.delay_enabled = doc["enabled"];
    }
    if (doc.containsKey("time")) {
      effects.delay_time_ms = doc["time"];
    }
    if (doc.containsKey("mix")) {
      effects.delay_mix = doc["mix"];
    }
    if (doc.containsKey("feedback")) {
      effects.delay_feedback = doc["feedback"];
    }
    if (doc.containsKey("tone")) {
      effects.delay_tone = doc["tone"];
    }
    
    // Send commands in the right order
    // 1. If parameters changed AND effect is enabled, send parameters first
    if (hasParams && effects.delay_enabled) {
      String cmd = "DLY:" + String(effects.delay_time_ms, 0) + "," + 
                   String(effects.delay_feedback, 2) + "," + 
                   String(effects.delay_mix, 2) + "," +
                   String(effects.delay_tone, 2);
      
      if (!sendCommandAndWaitForAck(cmd, 2000)) {
        success = false;
        message = "Failed to set delay parameters";
      }
      delay(100);
    }
    
    // 2. Then send toggle command if state changed
    if (hasToggle) {
      String cmd = effects.delay_enabled ? "DLY:ON" : "DLY:OFF";
      if (!sendCommandAndWaitForAck(cmd, 2000)) {
        success = false;
        message = "Failed to toggle delay";
      }
    }
    
    if (success) {
      server.send(200, "application/json", "{\"success\":true,\"message\":\"" + message + "\"}");
    } else {
      server.send(500, "application/json", "{\"success\":false,\"message\":\"" + message + "\"}");
    }
  }
}

void handleSetGate() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<300> doc;
    deserializeJson(doc, server.arg("plain"));
    
    bool success = true;
    String message = "Noise gate updated";
    bool hasToggle = doc.containsKey("enabled");
    bool hasParams = doc.containsKey("threshold") || doc.containsKey("attack") || doc.containsKey("release");
    
    // Update local effect state
    if (doc.containsKey("enabled")) {
      effects.gate_enabled = doc["enabled"];
    }
    if (doc.containsKey("threshold")) {
      effects.gate_threshold = doc["threshold"];
    }
    if (doc.containsKey("attack")) {
      effects.gate_attack = doc["attack"];
    }
    if (doc.containsKey("release")) {
      effects.gate_release = doc["release"];
    }
    
    // Send commands in the right order
    // 1. If parameters changed AND effect is enabled, send parameters first
    if (hasParams && effects.gate_enabled) {
      String cmd = "GATE:" + String(effects.gate_threshold, 3) + "," + 
                   String(effects.gate_attack, 4) + "," + 
                   String(effects.gate_release, 2);
      
      if (!sendCommandAndWaitForAck(cmd, 2000)) {
        success = false;
        message = "Failed to set gate parameters";
      }
      delay(100);
    }
    
    // 2. Then send toggle command if state changed
    if (hasToggle) {
      String cmd = effects.gate_enabled ? "GATE:ON" : "GATE:OFF";
      if (!sendCommandAndWaitForAck(cmd, 2000)) {
        success = false;
        message = "Failed to toggle noise gate";
      }
    }
    
    if (success) {
      server.send(200, "application/json", "{\"success\":true,\"message\":\"" + message + "\"}");
    } else {
      server.send(500, "application/json", "{\"success\":false,\"message\":\"" + message + "\"}");
    }
  }
}

void handleGetStatus() {
  StaticJsonDocument<500> doc;
  doc["volume"] = effects.volume;
  doc["overdrive"]["enabled"] = effects.overdrive_enabled;
  doc["overdrive"]["gain"] = effects.overdrive_gain;
  doc["delay"]["enabled"] = effects.delay_enabled;
  doc["delay"]["time_ms"] = effects.delay_time_ms;
  doc["delay"]["mix"] = effects.delay_mix;
  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

/**
 * Handle preset loading (UPDATED for optimized effects)
 */
void handleLoadPreset(String presetName) {
  // Define presets with REALISTIC parameters (no clipping!)
  if (presetName == "clean") {
    effects.volume = 0.7;
    effects.overdrive_enabled = false;
    effects.overdrive_gain = 1.0;      // Unity gain when disabled
    effects.overdrive_threshold = 0.8;
    effects.overdrive_tone = 0.7;
    effects.overdrive_mix = 0.2;       // Very subtle
    effects.overdrive_mode = 0;        // Soft
    effects.delay_enabled = false;
    effects.delay_time_ms = 100;
    effects.delay_feedback = 0.3;
    effects.delay_mix = 0.25;
    effects.delay_tone = 0.6;
    effects.gate_enabled = true;
    effects.gate_threshold = 0.015;    // Light gate
    effects.gate_attack = 0.001;
    effects.gate_release = 0.15;
  }
  else if (presetName == "crunch") {
    effects.volume = 0.6;              // Lower volume to prevent clipping
    effects.overdrive_enabled = true;
    effects.overdrive_gain = 8.0;      // Much lower gain (was 25!)
    effects.overdrive_threshold = 0.7;
    effects.overdrive_tone = 0.55;
    effects.overdrive_mix = 0.65;      // 65% wet, 35% clean
    effects.overdrive_mode = 2;        // Asymmetric (diode-like)
    effects.delay_enabled = false;
    effects.delay_time_ms = 75;
    effects.delay_feedback = 0.25;
    effects.delay_mix = 0.3;
    effects.delay_tone = 0.5;
    effects.gate_enabled = true;
    effects.gate_threshold = 0.02;
    effects.gate_attack = 0.001;
    effects.gate_release = 0.1;
  }
  else if (presetName == "lead") {
    effects.volume = 0.55;             // Even lower for high gain
    effects.overdrive_enabled = true;
    effects.overdrive_gain = 15.0;     // Moderate gain (was 50!)
    effects.overdrive_threshold = 0.6;
    effects.overdrive_tone = 0.65;     // Bright but not harsh
    effects.overdrive_mix = 0.8;       // 80% wet
    effects.overdrive_mode = 0;        // Soft (tube-like sustain)
    effects.delay_enabled = true;
    effects.delay_time_ms = 120;       // Slapback delay
    effects.delay_feedback = 0.35;
    effects.delay_mix = 0.35;
    effects.delay_tone = 0.4;          // Darker repeats
    effects.gate_enabled = true;
    effects.gate_threshold = 0.012;
    effects.gate_attack = 0.001;
    effects.gate_release = 0.2;        // Longer sustain
  }
  else if (presetName == "metal") {
    effects.volume = 0.5;              // Lowest volume (was 0.9!)
    effects.overdrive_enabled = true;
    effects.overdrive_gain = 20.0;     // High but not insane (was 80!)
    effects.overdrive_threshold = 0.4;
    effects.overdrive_tone = 0.3;      // Dark, heavy
    effects.overdrive_mix = 0.9;       // 90% wet
    effects.overdrive_mode = 1;        // Hard clipping
    effects.delay_enabled = false;
    effects.delay_time_ms = 50;
    effects.delay_feedback = 0.2;
    effects.delay_mix = 0.25;
    effects.delay_tone = 0.3;
    effects.gate_enabled = true;
    effects.gate_threshold = 0.025;    // Tight gate
    effects.gate_attack = 0.0005;      // Very fast
    effects.gate_release = 0.05;       // Quick cutoff
  }
  else if (presetName == "ambient") {
    effects.volume = 0.65;
    effects.overdrive_enabled = true;
    effects.overdrive_gain = 4.0;      // Very light (was 15!)
    effects.overdrive_threshold = 0.85;
    effects.overdrive_tone = 0.8;      // Bright, shimmery
    effects.overdrive_mix = 0.4;       // Mostly clean
    effects.overdrive_mode = 0;        // Soft
    effects.delay_enabled = true;
    effects.delay_time_ms = 250;       // Long delay
    effects.delay_feedback = 0.6;      // Lots of repeats
    effects.delay_mix = 0.55;          // Prominent delay
    effects.delay_tone = 0.7;          // Bright repeats
    effects.gate_enabled = false;      // No gate for ambient swells
    effects.gate_threshold = 0.01;
    effects.gate_attack = 0.001;
    effects.gate_release = 0.5;
  }
  else {
    server.send(404, "application/json", "{\"success\":false,\"message\":\"Preset not found\"}");
    return;
  }
  
  // Send all settings to STM32 with validation
  Serial.println("Loading preset: " + presetName);
  
  bool allSuccess = true;
  
  // 1. Set volume first
  if (!sendCommandAndWaitForAck("VOL:" + String(effects.volume, 2), 2000)) {
    Serial.println("Failed to set volume");
    allSuccess = false;
  }
  delay(100);  // Longer delay between commands
  
  // 2. Enable/disable overdrive FIRST, then set parameters
  if (!sendCommandAndWaitForAck(effects.overdrive_enabled ? "OVR:ON" : "OVR:OFF", 2000)) {
    Serial.println("Failed to toggle overdrive");
    allSuccess = false;
  }
  delay(100);
  
  // 3. Set overdrive parameters (only if enabled)
  if (effects.overdrive_enabled) {
    if (!sendCommandAndWaitForAck("OVR:" + String(effects.overdrive_gain, 1) + "," + 
                                  String(effects.overdrive_threshold, 2) + "," + 
                                  String(effects.overdrive_tone, 2) + "," +
                                  String(effects.overdrive_mix, 2) + "," +
                                  String(effects.overdrive_mode), 2000)) {
      Serial.println("Failed to set overdrive parameters");
      allSuccess = false;
    }
    delay(100);
  }
  
  // 4. Enable/disable delay FIRST
  if (!sendCommandAndWaitForAck(effects.delay_enabled ? "DLY:ON" : "DLY:OFF", 2000)) {
    Serial.println("Failed to toggle delay");
    allSuccess = false;
  }
  delay(100);
  
  // 5. Set delay parameters (only if enabled)
  if (effects.delay_enabled) {
    if (!sendCommandAndWaitForAck("DLY:" + String(effects.delay_time_ms, 0) + "," + 
                                  String(effects.delay_feedback, 2) + "," + 
                                  String(effects.delay_mix, 2) + "," +
                                  String(effects.delay_tone, 2), 2000)) {
      Serial.println("Failed to set delay parameters");
      allSuccess = false;
    }
    delay(100);
  }
  
  // 6. Enable/disable gate FIRST
  if (!sendCommandAndWaitForAck(effects.gate_enabled ? "GATE:ON" : "GATE:OFF", 2000)) {
    Serial.println("Failed to toggle gate");
    allSuccess = false;
  }
  delay(100);
  
  // 7. Set gate parameters (only if enabled)
  if (effects.gate_enabled) {
    if (!sendCommandAndWaitForAck("GATE:" + String(effects.gate_threshold, 3) + "," + 
                                  String(effects.gate_attack, 4) + "," + 
                                  String(effects.gate_release, 2), 2000)) {
      Serial.println("Failed to set gate parameters");
      allSuccess = false;
    }
  }
  
  if (allSuccess) {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Preset loaded: " + presetName + "\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Preset partially loaded (some commands failed)\"}");
  }
}

/**
 * Helper function to load preset by name
 */
void handleLoadPresetByName(String presetName) {
  handleLoadPreset(presetName);
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

/**
 * Sync with backend Node.js API (optional)
 */
void syncWithBackend() {
  HTTPClient http;
  http.begin(backend_url);
  
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<500> doc;
    
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      // Update from backend if needed
      if (doc.containsKey("volume")) {
        float vol = doc["volume"];
        if (vol != effects.volume) {
          effects.volume = vol;
          sendCommandAndWaitForAck("VOL:" + String(vol, 2), 1000);
        }
      }
    }
  }
  
  http.end();
}
