# STM32 DSP Guitar Pedal - Flutter Frontend

A professional Flutter mobile application for controlling the STM32 G431 DSP Guitar Pedal via Node.js backend API.

## 🎸 Features

- **Real-time Effect Control**: Adjust all DSP parameters with intuitive rotary knobs
- **Multiple Effects**:
  - Master Volume Control
  - Overdrive (with Gain, Threshold, Tone, and Mix controls)
  - Delay (with Time, Feedback, Mix, and Tone controls)
  - Noise Gate (with Threshold, Attack, and Release controls)
- **Preset System**: Quick access to 5 preset sounds (Clean, Crunch, Lead, Metal, Ambient)
- **Connection Management**: Real-time connection status and backend configuration
- **Offline Mode**: Control interface available even when disconnected

## 📱 Screenshots

The app features a dark, professional interface with:
- Color-coded rotary knobs for each effect parameter
- Toggle switches for enabling/disabling effects
- Preset buttons with visual feedback
- Connection status indicator
- Settings screen for backend configuration

## 🎛️ Effect Parameters & Knobs

### Master Volume
- **Range**: 0.0 - 1.0
- **Color**: Green
- **Purpose**: Overall output level control

### Overdrive Effect
- **Gain**: 1.0 - 100.0 (Orange knob)
  - Controls the amount of distortion/overdrive
- **Threshold**: 0.1 - 0.95 (Yellow knob)
  - Sets the clipping threshold
- **Tone**: 0.0 - 1.0 (Purple/Pink knob)
  - Adjusts the tone filter (low-pass cutoff)
- **Mix**: 0.0 - 1.0 (Blue knob)
  - Blends wet (effected) and dry (clean) signal

### Delay Effect
- **Time**: 20 - 100 ms (Cyan/Blue knob)
  - Sets the delay time in milliseconds
- **Feedback**: 0.0 - 0.95 (Green/Teal knob)
  - Controls how much delayed signal feeds back
- **Mix**: 0.0 - 1.0 (Light Green knob)
  - Blends delayed signal with dry signal
- **Tone**: 0.0 - 1.0 (Amber/Orange knob)
  - Adjusts the damping filter on delayed signal

### Noise Gate
- **Threshold**: 0.001 - 0.5 (Red knob)
  - Sets the gate opening threshold
- **Attack**: 0.0001 - 0.1 s (Yellow knob)
  - How quickly the gate opens
- **Release**: 0.01 - 1.0 s (Green knob)
  - How quickly the gate closes

## 🔧 Setup Instructions

### Prerequisites

1. **Flutter SDK**: Version 3.8.1 or higher
2. **Node.js Backend**: Running on your local network
3. **Network**: Mobile device and backend server on same WiFi

### Installation

1. **Install Dependencies**:
   ```bash
   cd Flutter/dsp_nucleo
   flutter pub get
   ```

2. **Configure Backend URL**:
   - Launch the app
   - Tap the settings icon (⚙️)
   - Enter your backend server URL (e.g., `http://192.168.1.100:3000`)
   - Tap "Test Connection" to verify
   - Tap "Save" to apply settings

3. **Run the App**:
   ```bash
   flutter run
   ```

### Backend Server Setup

Make sure the Node.js backend is running:

```bash
cd backend
npm install
npm start
```

The backend should be accessible at `http://<YOUR_IP>:3000`

## 📦 Dependencies

The app uses the following Flutter packages:

- **flutter_dial_knob** (^0.0.2): Rotary knob widgets for effect controls
- **dio** (^5.9.0): HTTP client for backend API communication
- **cupertino_icons** (^1.0.8): iOS-style icons

## 🏗️ Project Structure

```
lib/
├── main.dart                      # App entry point
├── models/
│   └── effect_state.dart         # Data models for effect parameters
├── screens/
│   ├── home_screen.dart          # Main control interface
│   └── settings_screen.dart      # Backend configuration
├── services/
│   └── api_service.dart          # Backend API communication
└── widgets/
    ├── effect_knob.dart          # Rotary knob widget
    └── effect_panel.dart         # Effect section container
```

## 🎨 Design Philosophy

The app features a **dark, professional guitar pedal interface** inspired by real hardware:

- **Color-coded knobs**: Each parameter has a unique color for quick visual identification
- **Visual feedback**: Connection status, effect enable/disable states
- **Responsive design**: Works on phones and tablets
- **Smooth animations**: Rotary knob interactions feel natural
- **Preset system**: Quick switching between common guitar tones

## 🔌 API Integration

The app communicates with the Node.js backend using REST API:

### Endpoints Used

- `GET /api/health` - Check backend availability
- `GET /api/effects` - Retrieve current effect settings
- `POST /api/volume` - Set master volume
- `POST /api/overdrive` - Configure overdrive parameters
- `POST /api/delay` - Configure delay parameters
- `POST /api/gate` - Configure noise gate parameters (if backend supports)
- `GET /api/presets` - List available presets
- `POST /api/presets/:name` - Load a preset

### Connection Flow

1. App attempts to connect to backend on startup
2. If successful, retrieves current effect settings
3. Knob changes send immediate API requests
4. Connection status displayed in app bar
5. Offline mode allows UI interaction without backend

## 🚀 Usage Tips

1. **Start with Clean Preset**: Use the "CLEAN" preset as a baseline
2. **Adjust Gain Carefully**: High overdrive gain can cause clipping
3. **Use Delay Sparingly**: Too much feedback creates oscillation
4. **Enable Noise Gate**: Reduces unwanted noise between playing
5. **Save Your Settings**: Backend maintains current state

## 🐛 Troubleshooting

### "Connection Failed" Error
- Verify backend server is running (`npm start`)
- Check both devices are on same WiFi network
- Confirm IP address is correct in settings
- Disable any VPN or firewall blocking port 3000

### Knobs Not Responding
- Check connection status indicator (top right)
- Tap refresh button to reconnect
- Verify backend API is responding (`http://<IP>:3000/api/health`)

### Audio Issues
- These occur at the STM32/ESP32 level, not in the Flutter app
- Check hardware connections and ESP32 serial monitor
- Verify UART communication between ESP32 and STM32

## 📱 Platform Support

- ✅ Android
- ✅ iOS
- ✅ Web (with CORS enabled on backend)
- ⚠️ Desktop (requires network configuration)

## 🔮 Future Enhancements

- [ ] Save custom presets locally
- [ ] Bluetooth direct connection to ESP32
- [ ] Tuner functionality
- [ ] Waveform visualization
- [ ] Recording/playback features
- [ ] MIDI controller support

## 📄 License

MIT License - See LICENSE file for details

## 👨‍💻 Development

Built with ❤️ using Flutter for the STM32 G431 DSP Guitar Pedal project.

**Hardware Stack:**
- STM32 G431RB (DSP Processing)
- ESP32 (WiFi Bridge)
- Node.js Backend (API Layer)
- Flutter Frontend (Mobile Control)

---

**Need Help?** Check the backend README or open an issue on the project repository.
