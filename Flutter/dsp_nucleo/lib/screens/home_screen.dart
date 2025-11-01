import 'package:flutter/material.dart';
import 'package:dsp_nucleo/services/api_service.dart';
import 'package:dsp_nucleo/models/effect_state.dart';
import 'package:dsp_nucleo/widgets/effect_panel.dart';
import 'package:dsp_nucleo/widgets/effect_knob.dart';
import 'package:dsp_nucleo/screens/settings_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({Key? key}) : super(key: key);

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final ApiService _apiService = ApiService();
  EffectState _effectState = EffectState();
  bool _isConnected = false;
  bool _isLoading = true;

  @override
  void initState() {
    super.initState();
    // Default backend URL - user can change in settings
    _apiService.setBackendUrl('http://192.168.1.100:3000');
    _connectToBackend();
  }

  Future<void> _connectToBackend() async {
    setState(() {
      _isLoading = true;
    });

    final connected = await _apiService.checkConnection();
    
    if (connected) {
      final effects = await _apiService.getEffects();
      if (effects != null) {
        setState(() {
          _effectState = EffectState.fromJson(effects);
          _isConnected = true;
          _isLoading = false;
        });
        return;
      }
    }

    setState(() {
      _isConnected = false;
      _isLoading = false;
    });
  }

  void _showSnackBar(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        duration: const Duration(seconds: 2),
        backgroundColor: Colors.grey[800],
      ),
    );
  }

  Future<void> _loadPreset(String presetName) async {
    if (!_isConnected) {
      _showSnackBar('Not connected to backend');
      return;
    }

    final success = await _apiService.loadPreset(presetName);
    if (success) {
      _showSnackBar('Preset "$presetName" loaded');
      // Refresh state
      final effects = await _apiService.getEffects();
      if (effects != null) {
        setState(() {
          _effectState = EffectState.fromJson(effects);
        });
      }
    } else {
      _showSnackBar('Failed to load preset');
    }
  }

  Widget _buildPresetButtons() {
    final presets = [
      {'name': 'clean', 'icon': Icons.clean_hands, 'color': Colors.blueAccent},
      {'name': 'crunch', 'icon': Icons.power, 'color': Colors.orangeAccent},
      {'name': 'lead', 'icon': Icons.electric_bolt, 'color': Colors.yellowAccent},
      {'name': 'metal', 'icon': Icons.whatshot, 'color': Colors.redAccent},
      {'name': 'ambient', 'icon': Icons.cloud, 'color': Colors.purpleAccent},
    ];

    return Container(
      margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.grey[900],
        borderRadius: BorderRadius.circular(14),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.5),
            blurRadius: 8,
            spreadRadius: 1,
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'Presets',
            style: TextStyle(
              color: Colors.white,
              fontSize: 17,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 10),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: presets.map((preset) {
              return ElevatedButton.icon(
                onPressed: _isConnected
                    ? () => _loadPreset(preset['name'] as String)
                    : null,
                icon: Icon(preset['icon'] as IconData, size: 16),
                label: Text(
                  (preset['name'] as String).toUpperCase(),
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                    fontSize: 11,
                  ),
                ),
                style: ElevatedButton.styleFrom(
                  backgroundColor: _isConnected
                      ? preset['color'] as Color
                      : Colors.grey,
                  foregroundColor: Colors.black,
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(16),
                  ),
                ),
              );
            }).toList(),
          ),
        ],
      ),
    );
  }

  Widget _buildVolumeControl() {
    return Container(
      margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Colors.grey[900]!, Colors.grey[850]!],
        ),
        borderRadius: BorderRadius.circular(16),
        boxShadow: [
          BoxShadow(
            color: Colors.greenAccent.withOpacity(0.3),
            blurRadius: 12,
            spreadRadius: 1,
          ),
        ],
      ),
      child: Row(
        children: [
          const Icon(Icons.volume_up, color: Colors.greenAccent, size: 28),
          const SizedBox(width: 12),
          const Text(
            'Master Volume',
            style: TextStyle(
              color: Colors.white,
              fontSize: 16,
              fontWeight: FontWeight.bold,
            ),
          ),
          const Spacer(),
          SizedBox(
            width: 90,
            child: EffectKnob(
              label: '',
              value: _effectState.volume,
              min: 0.0,
              max: 1.0,
              decimals: 2,
              onChanged: (value) async {
                setState(() {
                  _effectState.volume = value;
                });
                if (_isConnected) {
                  await _apiService.setVolume(value);
                }
              },
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildOverdrivePanel() {
    return EffectPanel(
      title: 'Overdrive',
      icon: Icons.graphic_eq,
      enabled: _effectState.overdrive.enabled,
      onToggle: () async {
        setState(() {
          _effectState.overdrive.enabled = !_effectState.overdrive.enabled;
        });
        if (_isConnected) {
          await _apiService.setOverdrive(
            enabled: _effectState.overdrive.enabled,
          );
        }
      },
      children: [
        EffectKnob(
          label: 'Gain',
          value: _effectState.overdrive.gain,
          min: 1.0,
          max: 100.0,
          decimals: 1,
          onChanged: (value) async {
            setState(() {
              _effectState.overdrive.gain = value;
            });
            if (_isConnected) {
              await _apiService.setOverdrive(gain: value);
            }
          },
          levelColorStart: Colors.orangeAccent,
          levelColorEnd: Colors.deepOrangeAccent,
        ),
        EffectKnob(
          label: 'Threshold',
          value: _effectState.overdrive.threshold,
          min: 0.1,
          max: 0.95,
          decimals: 2,
          onChanged: (value) async {
            setState(() {
              _effectState.overdrive.threshold = value;
            });
            if (_isConnected) {
              await _apiService.setOverdrive(threshold: value);
            }
          },
          levelColorStart: Colors.yellowAccent,
          levelColorEnd: Colors.orangeAccent,
        ),
        EffectKnob(
          label: 'Tone',
          value: _effectState.overdrive.tone,
          min: 0.0,
          max: 1.0,
          decimals: 2,
          onChanged: (value) async {
            setState(() {
              _effectState.overdrive.tone = value;
            });
            if (_isConnected) {
              await _apiService.setOverdrive(tone: value);
            }
          },
          levelColorStart: Colors.purpleAccent,
          levelColorEnd: Colors.pinkAccent,
        ),
        EffectKnob(
          label: 'Mix',
          value: _effectState.overdrive.mix,
          min: 0.0,
          max: 1.0,
          decimals: 2,
          onChanged: (value) async {
            setState(() {
              _effectState.overdrive.mix = value;
            });
            if (_isConnected) {
              await _apiService.setOverdrive(mix: value);
            }
          },
          levelColorStart: Colors.lightBlueAccent,
          levelColorEnd: Colors.blueAccent,
        ),
      ],
    );
  }

  Widget _buildDelayPanel() {
    return EffectPanel(
      title: 'Delay',
      icon: Icons.repeat,
      enabled: _effectState.delay.enabled,
      onToggle: () async {
        setState(() {
          _effectState.delay.enabled = !_effectState.delay.enabled;
        });
        if (_isConnected) {
          await _apiService.setDelay(enabled: _effectState.delay.enabled);
        }
      },
      children: [
        EffectKnob(
          label: 'Time',
          value: _effectState.delay.timeMs,
          min: 20.0,
          max: 100.0,
          decimals: 0,
          unit: 'ms',
          onChanged: (value) async {
            setState(() {
              _effectState.delay.timeMs = value;
            });
            if (_isConnected) {
              await _apiService.setDelay(timeMs: value);
            }
          },
          levelColorStart: Colors.cyanAccent,
          levelColorEnd: Colors.blueAccent,
        ),
        EffectKnob(
          label: 'Feedback',
          value: _effectState.delay.feedback,
          min: 0.0,
          max: 0.95,
          decimals: 2,
          onChanged: (value) async {
            setState(() {
              _effectState.delay.feedback = value;
            });
            if (_isConnected) {
              await _apiService.setDelay(feedback: value);
            }
          },
          levelColorStart: Colors.greenAccent,
          levelColorEnd: Colors.tealAccent,
        ),
        EffectKnob(
          label: 'Mix',
          value: _effectState.delay.mix,
          min: 0.0,
          max: 1.0,
          decimals: 2,
          onChanged: (value) async {
            setState(() {
              _effectState.delay.mix = value;
            });
            if (_isConnected) {
              await _apiService.setDelay(mix: value);
            }
          },
          levelColorStart: Colors.lightGreenAccent,
          levelColorEnd: Colors.greenAccent,
        ),
        EffectKnob(
          label: 'Tone',
          value: _effectState.delay.tone,
          min: 0.0,
          max: 1.0,
          decimals: 2,
          onChanged: (value) async {
            setState(() {
              _effectState.delay.tone = value;
            });
            if (_isConnected) {
              await _apiService.setDelay(tone: value);
            }
          },
          levelColorStart: Colors.amberAccent,
          levelColorEnd: Colors.orangeAccent,
        ),
      ],
    );
  }

  Widget _buildNoiseGatePanel() {
    return EffectPanel(
      title: 'Noise Gate',
      icon: Icons.gavel,
      enabled: _effectState.noiseGate.enabled,
      onToggle: () async {
        setState(() {
          _effectState.noiseGate.enabled = !_effectState.noiseGate.enabled;
        });
        if (_isConnected) {
          await _apiService.setNoiseGate(enabled: _effectState.noiseGate.enabled);
        }
      },
      children: [
        EffectKnob(
          label: 'Threshold',
          value: _effectState.noiseGate.threshold,
          min: 0.001,
          max: 0.5,
          decimals: 3,
          onChanged: (value) async {
            setState(() {
              _effectState.noiseGate.threshold = value;
            });
            if (_isConnected) {
              await _apiService.setNoiseGate(threshold: value);
            }
          },
          levelColorStart: Colors.redAccent,
          levelColorEnd: Colors.deepOrangeAccent,
        ),
        EffectKnob(
          label: 'Attack',
          value: _effectState.noiseGate.attack,
          min: 0.0001,
          max: 0.1,
          decimals: 4,
          unit: 's',
          onChanged: (value) async {
            setState(() {
              _effectState.noiseGate.attack = value;
            });
            if (_isConnected) {
              await _apiService.setNoiseGate(attack: value);
            }
          },
          levelColorStart: Colors.yellowAccent,
          levelColorEnd: Colors.amberAccent,
        ),
        EffectKnob(
          label: 'Release',
          value: _effectState.noiseGate.release,
          min: 0.01,
          max: 1.0,
          decimals: 2,
          unit: 's',
          onChanged: (value) async {
            setState(() {
              _effectState.noiseGate.release = value;
            });
            if (_isConnected) {
              await _apiService.setNoiseGate(release: value);
            }
          },
          levelColorStart: Colors.lightGreenAccent,
          levelColorEnd: Colors.greenAccent,
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Row(
          children: [
            Icon(Icons.music_note, color: Colors.greenAccent, size: 22),
            SizedBox(width: 6),
            Text(
              'STM32 DSP Pedal',
              style: TextStyle(fontSize: 17),
            ),
          ],
        ),
        actions: [
          // Connection status indicator
          Container(
            margin: const EdgeInsets.only(right: 4),
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
            decoration: BoxDecoration(
              color: _isConnected ? Colors.greenAccent : Colors.redAccent,
              borderRadius: BorderRadius.circular(12),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(
                  _isConnected ? Icons.wifi : Icons.wifi_off,
                  size: 14,
                  color: Colors.black,
                ),
                const SizedBox(width: 3),
                Text(
                  _isConnected ? 'ON' : 'OFF',
                  style: const TextStyle(
                    color: Colors.black,
                    fontSize: 11,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
          ),
          // Refresh button
          IconButton(
            icon: const Icon(Icons.refresh, size: 22),
            onPressed: _connectToBackend,
            tooltip: 'Reconnect',
          ),
          // Settings button
          IconButton(
            icon: const Icon(Icons.settings, size: 22),
            onPressed: () async {
              final result = await Navigator.push(
                context,
                MaterialPageRoute(
                  builder: (context) => SettingsScreen(apiService: _apiService),
                ),
              );
              if (result == true) {
                _connectToBackend();
              }
            },
            tooltip: 'Settings',
          ),
        ],
      ),
      body: _isLoading
          ? const Center(
              child: CircularProgressIndicator(
                color: Colors.greenAccent,
              ),
            )
          : SafeArea(
              child: SingleChildScrollView(
                child: Column(
                  children: [
                    const SizedBox(height: 6),
                    _buildPresetButtons(),
                    _buildVolumeControl(),
                    _buildOverdrivePanel(),
                    _buildDelayPanel(),
                    _buildNoiseGatePanel(),
                    const SizedBox(height: 16),
                  ],
                ),
              ),
            ),
    );
  }
}
