/**
 * STM32 DSP Control API
 * 
 * This Node.js server provides a REST API to control the STM32 DSP.
 * The ESP32 polls this server for updates and forwards commands to STM32 via UART.
 * 
 * Architecture:
 * Flutter App -> Backend API (this server) <- ESP32 (polls) -> STM32 (UART)
 * 
 * Endpoints:
 * - GET  /api/effects       - Get current effect settings (ESP32 polls this)
 * - POST /api/effects       - Update all effect settings
 * - POST /api/volume        - Set output volume
 * - POST /api/overdrive     - Configure overdrive effect
 * - POST /api/delay         - Configure delay effect
 * - POST /api/gate          - Configure noise gate
 * - GET  /api/presets       - List available presets
 * - POST /api/presets/:name - Load a preset
 */

const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

// In-memory state (ESP32 polls this for updates)
let currentEffects = {
  volume: 0.7,
  overdrive: {
    enabled: false,
    gain: 5.0,
    threshold: 0.7,
    tone: 0.5,
    mix: 0.5,
    mode: 0
  },
  delay: {
    enabled: false,
    time_ms: 100,
    feedback: 0.3,
    mix: 0.3,
    tone: 0.5
  },
  gate: {
    enabled: true,
    threshold: 0.015,
    attack: 0.001,
    release: 0.15
  }
};

// Presets - complete effect configurations
const presets = {
  clean: {
    volume: 0.7,
    overdrive: { enabled: false, gain: 1.0, threshold: 0.8, tone: 0.7, mix: 0.2, mode: 0 },
    delay: { enabled: false, time_ms: 100, feedback: 0.3, mix: 0.25, tone: 0.6 },
    gate: { enabled: true, threshold: 0.015, attack: 0.001, release: 0.15 }
  },
  crunch: {
    volume: 0.6,
    overdrive: { enabled: true, gain: 8.0, threshold: 0.7, tone: 0.55, mix: 0.65, mode: 2 },
    delay: { enabled: false, time_ms: 75, feedback: 0.25, mix: 0.3, tone: 0.5 },
    gate: { enabled: true, threshold: 0.02, attack: 0.001, release: 0.1 }
  },
  lead: {
    volume: 0.55,
    overdrive: { enabled: true, gain: 15.0, threshold: 0.6, tone: 0.65, mix: 0.8, mode: 0 },
    delay: { enabled: true, time_ms: 120, feedback: 0.35, mix: 0.35, tone: 0.4 },
    gate: { enabled: true, threshold: 0.012, attack: 0.001, release: 0.2 }
  },
  ambient: {
    volume: 0.65,
    overdrive: { enabled: true, gain: 4.0, threshold: 0.85, tone: 0.8, mix: 0.4, mode: 0 },
    delay: { enabled: true, time_ms: 250, feedback: 0.6, mix: 0.55, tone: 0.7 },
    gate: { enabled: false, threshold: 0.01, attack: 0.001, release: 0.5 }
  },
  metal: {
    volume: 0.5,
    overdrive: { enabled: true, gain: 20.0, threshold: 0.4, tone: 0.3, mix: 0.9, mode: 1 },
    delay: { enabled: false, time_ms: 50, feedback: 0.2, mix: 0.25, tone: 0.3 },
    gate: { enabled: true, threshold: 0.025, attack: 0.0005, release: 0.05 }
  }
};

// Routes

// Health check
app.get('/api/health', (req, res) => {
  res.json({ 
    status: 'ok', 
    timestamp: new Date().toISOString(),
    architecture: 'Flutter -> Backend -> ESP32 (poll) -> STM32'
  });
});

// Get current effects settings (ESP32 polls this endpoint)
app.get('/api/effects', (req, res) => {
  res.json({
    success: true,
    effects: currentEffects
  });
});

// Update all effects settings
app.post('/api/effects', (req, res) => {
  const { volume, overdrive, delay, gate } = req.body;
  
  // Update volume
  if (volume !== undefined) {
    currentEffects.volume = volume;
  }
  
  // Update overdrive
  if (overdrive) {
    currentEffects.overdrive = { ...currentEffects.overdrive, ...overdrive };
  }
  
  // Update delay
  if (delay) {
    currentEffects.delay = { ...currentEffects.delay, ...delay };
  }
  
  // Update gate
  if (gate) {
    currentEffects.gate = { ...currentEffects.gate, ...gate };
  }
  
  console.log('✓ Effects updated:', { volume, overdrive, delay, gate });
  
  res.json({
    success: true,
    message: 'Effects updated successfully',
    effects: currentEffects
  });
});

// Set volume
app.post('/api/volume', (req, res) => {
  const { volume } = req.body;
  
  if (volume === undefined || volume < 0 || volume > 1) {
    return res.status(400).json({ 
      success: false, 
      message: 'Volume must be between 0 and 1' 
    });
  }
  
  currentEffects.volume = volume;
  console.log(`✓ Volume set to ${(volume * 100).toFixed(0)}%`);
  
  res.json({
    success: true,
    message: `Volume set to ${(volume * 100).toFixed(0)}%`,
    volume: currentEffects.volume
  });
});

// Configure overdrive
app.post('/api/overdrive', (req, res) => {
  const { enabled, gain, threshold, tone, mix, mode } = req.body;
  
  const overdriveData = {};
  if (enabled !== undefined) overdriveData.enabled = enabled;
  if (gain !== undefined && gain >= 1.0 && gain <= 100.0) overdriveData.gain = gain;
  if (threshold !== undefined && threshold >= 0.1 && threshold <= 0.95) overdriveData.threshold = threshold;
  if (tone !== undefined && tone >= 0.0 && tone <= 1.0) overdriveData.tone = tone;
  if (mix !== undefined && mix >= 0.0 && mix <= 1.0) overdriveData.mix = mix;
  if (mode !== undefined && mode >= 0 && mode <= 2) overdriveData.mode = mode;
  
  currentEffects.overdrive = { ...currentEffects.overdrive, ...overdriveData };
  console.log('✓ Overdrive updated:', overdriveData);
  
  res.json({
    success: true,
    message: 'Overdrive updated',
    overdrive: currentEffects.overdrive
  });
});

// Configure delay
app.post('/api/delay', (req, res) => {
  const { enabled, time_ms, feedback, mix, tone } = req.body;
  
  const delayData = {};
  if (enabled !== undefined) delayData.enabled = enabled;
  if (time_ms !== undefined && time_ms >= 20 && time_ms <= 500) delayData.time_ms = time_ms;
  if (feedback !== undefined && feedback >= 0.0 && feedback <= 0.95) delayData.feedback = feedback;
  if (mix !== undefined && mix >= 0.0 && mix <= 1.0) delayData.mix = mix;
  if (tone !== undefined && tone >= 0.0 && tone <= 1.0) delayData.tone = tone;
  
  currentEffects.delay = { ...currentEffects.delay, ...delayData };
  console.log('✓ Delay updated:', delayData);
  
  res.json({
    success: true,
    message: 'Delay updated',
    delay: currentEffects.delay
  });
});

// Configure noise gate
app.post('/api/gate', (req, res) => {
  const { enabled, threshold, attack, release } = req.body;
  
  const gateData = {};
  if (enabled !== undefined) gateData.enabled = enabled;
  if (threshold !== undefined && threshold >= 0.001 && threshold <= 0.5) gateData.threshold = threshold;
  if (attack !== undefined && attack >= 0.0001 && attack <= 0.1) gateData.attack = attack;
  if (release !== undefined && release >= 0.01 && release <= 1.0) gateData.release = release;
  
  currentEffects.gate = { ...currentEffects.gate, ...gateData };
  console.log('✓ Noise gate updated:', gateData);
  
  res.json({
    success: true,
    message: 'Noise gate updated',
    gate: currentEffects.gate
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
app.post('/api/presets/:name', (req, res) => {
  const { name } = req.params;
  
  if (!presets[name]) {
    return res.status(404).json({
      success: false,
      message: `Preset '${name}' not found`
    });
  }
  
  const preset = presets[name];
  
  // Update current effects with preset values
  currentEffects = JSON.parse(JSON.stringify(preset));
  
  console.log(`✓ Preset '${name}' loaded`);
  
  res.json({
    success: true,
    message: `Preset '${name}' loaded`,
    preset: name,
    effects: currentEffects
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
  console.log(`\n╔════════════════════════════════════════╗`);
  console.log(`║  STM32 DSP Control API Server          ║`);
  console.log(`╚════════════════════════════════════════╝`);
  console.log(`\n📡 Architecture:`);
  console.log(`   Flutter App → Backend API → ESP32 → STM32`);
  console.log(`                 (this server) (polls)`);
  console.log(`\n🌐 Server running on port ${PORT}`);
  console.log(`\n📋 API Endpoints:`);
  console.log(`   GET  http://localhost:${PORT}/api/health`);
  console.log(`   GET  http://localhost:${PORT}/api/effects     (ESP32 polls this)`);
  console.log(`   POST http://localhost:${PORT}/api/effects`);
  console.log(`   POST http://localhost:${PORT}/api/volume`);
  console.log(`   POST http://localhost:${PORT}/api/overdrive`);
  console.log(`   POST http://localhost:${PORT}/api/delay`);
  console.log(`   POST http://localhost:${PORT}/api/gate`);
  console.log(`   GET  http://localhost:${PORT}/api/presets`);
  console.log(`   POST http://localhost:${PORT}/api/presets/:name`);
  console.log(`\n✓ Ready to accept commands from Flutter app`);
  console.log(`✓ ESP32 should poll GET /api/effects every 500ms\n`);
});

// Export for testing
module.exports = app;
