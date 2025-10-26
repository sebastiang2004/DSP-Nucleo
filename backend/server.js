/**
 * STM32 DSP Control API
 * 
 * This Node.js server provides a REST API to control the STM32 DSP
 * through the ESP32 WiFi bridge.
 * 
 * Endpoints:
 * - GET  /api/effects       - Get current effect settings
 * - POST /api/effects       - Update all effect settings
 * - POST /api/volume        - Set output volume
 * - POST /api/overdrive     - Configure overdrive effect
 * - POST /api/delay         - Configure delay effect
 * - GET  /api/presets       - List available presets
 * - POST /api/presets/:name - Load a preset
 */

const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const axios = require('axios');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// ESP32 IP address (configure this based on your network)
const ESP32_IP = process.env.ESP32_IP || '192.168.1.100';
const ESP32_URL = `http://${ESP32_IP}`;

// Middleware
app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

// In-memory state (synced with ESP32/STM32)
let currentEffects = {
  volume: 0.7,
  overdrive: {
    enabled: false,
    gain: 5.0,
    threshold: 0.7,
    tone: 0.5
  },
  delay: {
    enabled: false,
    time_ms: 100,
    feedback: 0.3,
    mix: 0.3
  }
};

// Presets - synced with ESP32 values (realistic, non-clipping gains)
const presets = {
  clean: {
    volume: 0.75,
    overdrive: { enabled: false, gain: 1.0, threshold: 0.9, tone: 0.5 },
    delay: { enabled: false, time_ms: 50, feedback: 0.25, mix: 0.25 }
  },
  crunch: {
    volume: 0.6,
    overdrive: { enabled: true, gain: 8.0, threshold: 0.65, tone: 0.5 },
    delay: { enabled: false, time_ms: 60, feedback: 0.3, mix: 0.3 }
  },
  lead: {
    volume: 0.55,
    overdrive: { enabled: true, gain: 15.0, threshold: 0.5, tone: 0.6 },
    delay: { enabled: true, time_ms: 80, feedback: 0.4, mix: 0.4 }
  },
  ambient: {
    volume: 0.65,
    overdrive: { enabled: true, gain: 4.0, threshold: 0.75, tone: 0.7 },
    delay: { enabled: true, time_ms: 100, feedback: 0.55, mix: 0.6 }
  },
  metal: {
    volume: 0.5,
    overdrive: { enabled: true, gain: 20.0, threshold: 0.4, tone: 0.3 },
    delay: { enabled: false, time_ms: 50, feedback: 0.2, mix: 0.2 }
  }
};

// Helper function to communicate with ESP32
async function sendToESP32(endpoint, data) {
  try {
    const response = await axios.post(`${ESP32_URL}${endpoint}`, data, {
      timeout: 5000,
      headers: { 'Content-Type': 'application/json' }
    });
    return { success: true, data: response.data };
  } catch (error) {
    console.error(`Error communicating with ESP32: ${error.message}`);
    return { success: false, error: error.message };
  }
}

async function getFromESP32(endpoint) {
  try {
    const response = await axios.get(`${ESP32_URL}${endpoint}`, {
      timeout: 5000
    });
    return { success: true, data: response.data };
  } catch (error) {
    console.error(`Error communicating with ESP32: ${error.message}`);
    return { success: false, error: error.message };
  }
}

// Routes

// Health check
app.get('/api/health', (req, res) => {
  res.json({ 
    status: 'ok', 
    timestamp: new Date().toISOString(),
    esp32_ip: ESP32_IP
  });
});

// Get current effects settings
app.get('/api/effects', async (req, res) => {
  // Try to sync with ESP32
  const result = await getFromESP32('/api/status');
  
  if (result.success) {
    currentEffects = result.data;
  }
  
  res.json({
    success: true,
    effects: currentEffects,
    synced: result.success
  });
});

// Update all effects settings
app.post('/api/effects', async (req, res) => {
  const { volume, overdrive, delay } = req.body;
  
  const results = [];
  
  // Update volume
  if (volume !== undefined) {
    const result = await sendToESP32('/api/volume', { volume });
    results.push({ type: 'volume', ...result });
    if (result.success) currentEffects.volume = volume;
  }
  
  // Update overdrive
  if (overdrive) {
    const result = await sendToESP32('/api/overdrive', overdrive);
    results.push({ type: 'overdrive', ...result });
    if (result.success) currentEffects.overdrive = { ...currentEffects.overdrive, ...overdrive };
  }
  
  // Update delay
  if (delay) {
    const result = await sendToESP32('/api/delay', delay);
    results.push({ type: 'delay', ...result });
    if (result.success) currentEffects.delay = { ...currentEffects.delay, ...delay };
  }
  
  const allSuccess = results.every(r => r.success);
  
  res.json({
    success: allSuccess,
    message: allSuccess ? 'Effects updated successfully' : 'Some updates failed',
    results,
    currentEffects
  });
});

// Set volume
app.post('/api/volume', async (req, res) => {
  const { volume } = req.body;
  
  if (volume === undefined || volume < 0 || volume > 1) {
    return res.status(400).json({ 
      success: false, 
      message: 'Volume must be between 0 and 1' 
    });
  }
  
  const result = await sendToESP32('/api/volume', { volume });
  
  if (result.success) {
    currentEffects.volume = volume;
  }
  
  res.json({
    success: result.success,
    message: result.success ? `Volume set to ${(volume * 100).toFixed(0)}%` : 'Failed to set volume',
    volume: currentEffects.volume
  });
});

// Configure overdrive
app.post('/api/overdrive', async (req, res) => {
  const { enabled, gain, threshold, tone } = req.body;
  
  const overdriveData = {};
  if (enabled !== undefined) overdriveData.enabled = enabled;
  if (gain !== undefined && gain >= 1.0 && gain <= 30.0) overdriveData.gain = gain;
  if (threshold !== undefined && threshold >= 0.1 && threshold <= 0.95) overdriveData.threshold = threshold;
  if (tone !== undefined && tone >= 0.0 && tone <= 1.0) overdriveData.tone = tone;
  
  const result = await sendToESP32('/api/overdrive', overdriveData);
  
  if (result.success) {
    currentEffects.overdrive = { ...currentEffects.overdrive, ...overdriveData };
  }
  
  res.json({
    success: result.success,
    message: result.success ? 'Overdrive updated' : 'Failed to update overdrive',
    overdrive: currentEffects.overdrive
  });
});

// Configure delay
app.post('/api/delay', async (req, res) => {
  const { enabled, time_ms, feedback, mix } = req.body;
  
  const delayData = {};
  if (enabled !== undefined) delayData.enabled = enabled;
  if (time_ms !== undefined && time_ms >= 20 && time_ms <= 100) delayData.time = time_ms;
  if (feedback !== undefined && feedback >= 0.0 && feedback <= 0.95) delayData.feedback = feedback;
  if (mix !== undefined && mix >= 0.0 && mix <= 1.0) delayData.mix = mix;
  
  const result = await sendToESP32('/api/delay', delayData);
  
  if (result.success) {
    if (delayData.time !== undefined) delayData.time_ms = delayData.time;
    delete delayData.time;
    currentEffects.delay = { ...currentEffects.delay, ...delayData };
  }
  
  res.json({
    success: result.success,
    message: result.success ? 'Delay updated' : 'Failed to update delay',
    delay: currentEffects.delay
  });
});

// List presets
app.get('/api/presets', (req, res) => {
  res.json({
    success: true,
    presets: Object.keys(presets).map(name => ({
      name,
      description: getPresetDescription(name)
    }))
  });
});

// Load a preset
app.post('/api/presets/:name', async (req, res) => {
  const { name } = req.params;
  
  if (!presets[name]) {
    return res.status(404).json({
      success: false,
      message: `Preset '${name}' not found`
    });
  }
  
  const preset = presets[name];
  const results = [];
  
  // Apply volume
  const volResult = await sendToESP32('/api/volume', { volume: preset.volume });
  results.push({ type: 'volume', ...volResult });
  
  // Apply overdrive
  const ovrResult = await sendToESP32('/api/overdrive', preset.overdrive);
  results.push({ type: 'overdrive', ...ovrResult });
  
  // Apply delay
  const dlyResult = await sendToESP32('/api/delay', {
    enabled: preset.delay.enabled,
    time: preset.delay.time_ms,
    feedback: preset.delay.feedback,
    mix: preset.delay.mix
  });
  results.push({ type: 'delay', ...dlyResult });
  
  const allSuccess = results.every(r => r.success);
  
  if (allSuccess) {
    currentEffects = JSON.parse(JSON.stringify(preset));
  }
  
  res.json({
    success: allSuccess,
    message: allSuccess ? `Preset '${name}' loaded` : 'Failed to load preset',
    preset: name,
    results,
    currentEffects
  });
});

// Helper function for preset descriptions
function getPresetDescription(name) {
  const descriptions = {
    clean: 'Crystal clear tone with minimal processing',
    crunch: 'Classic rock crunch with medium gain',
    lead: 'High gain lead tone with delay',
    ambient: 'Spacey atmospheric sound with heavy delay',
    metal: 'Heavy distortion for metal riffs'
  };
  return descriptions[name] || '';
}

// Start server
app.listen(PORT, () => {
  console.log(`\nSTM32 DSP Control API Server`);
  console.log(`━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`);
  console.log(`Server running on port ${PORT}`);
  console.log(`ESP32 IP: ${ESP32_IP}`);
  console.log(`\nAPI Endpoints:`);
  console.log(`  GET  http://localhost:${PORT}/api/health`);
  console.log(`  GET  http://localhost:${PORT}/api/effects`);
  console.log(`  POST http://localhost:${PORT}/api/volume`);
  console.log(`  POST http://localhost:${PORT}/api/overdrive`);
  console.log(`  POST http://localhost:${PORT}/api/delay`);
  console.log(`  GET  http://localhost:${PORT}/api/presets`);
  console.log(`  POST http://localhost:${PORT}/api/presets/:name`);
  console.log(`\nReady\n`);
});

// Export for testing
module.exports = app;
