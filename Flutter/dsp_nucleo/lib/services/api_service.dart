import 'package:dio/dio.dart';

class ApiService {
  final Dio _dio = Dio();
  late String _baseUrl;
  bool _isConnected = false;

  ApiService() {
    _dio.options.connectTimeout = const Duration(seconds: 5);
    _dio.options.receiveTimeout = const Duration(seconds: 5);
  }

  void setBackendUrl(String url) {
    _baseUrl = url;
  }

  bool get isConnected => _isConnected;

  // Health check
  Future<bool> checkConnection() async {
    try {
      final response = await _dio.get('$_baseUrl/api/health');
      _isConnected = response.statusCode == 200;
      return _isConnected;
    } catch (e) {
      _isConnected = false;
      return false;
    }
  }

  // Get current effects
  Future<Map<String, dynamic>?> getEffects() async {
    try {
      final response = await _dio.get('$_baseUrl/api/effects');
      if (response.statusCode == 200) {
        return response.data['effects'];
      }
      return null;
    } catch (e) {
      print('Error getting effects: $e');
      return null;
    }
  }

  // Set volume
  Future<bool> setVolume(double volume) async {
    try {
      final response = await _dio.post(
        '$_baseUrl/api/volume',
        data: {'volume': volume},
      );
      return response.statusCode == 200;
    } catch (e) {
      print('Error setting volume: $e');
      return false;
    }
  }

  // Set overdrive
  Future<bool> setOverdrive({
    bool? enabled,
    double? gain,
    double? threshold,
    double? tone,
    double? mix,
    int? mode,
  }) async {
    try {
      final data = <String, dynamic>{};
      if (enabled != null) data['enabled'] = enabled;
      if (gain != null) data['gain'] = gain;
      if (threshold != null) data['threshold'] = threshold;
      if (tone != null) data['tone'] = tone;
      if (mix != null) data['mix'] = mix;
      if (mode != null) data['mode'] = mode;

      final response = await _dio.post(
        '$_baseUrl/api/overdrive',
        data: data,
      );
      return response.statusCode == 200;
    } catch (e) {
      print('Error setting overdrive: $e');
      return false;
    }
  }

  // Set delay
  Future<bool> setDelay({
    bool? enabled,
    double? timeMs,
    double? feedback,
    double? mix,
    double? tone,
  }) async {
    try {
      final data = <String, dynamic>{};
      if (enabled != null) data['enabled'] = enabled;
      if (timeMs != null) data['time_ms'] = timeMs;
      if (feedback != null) data['feedback'] = feedback;
      if (mix != null) data['mix'] = mix;
      if (tone != null) data['tone'] = tone;

      final response = await _dio.post(
        '$_baseUrl/api/delay',
        data: data,
      );
      return response.statusCode == 200;
    } catch (e) {
      print('Error setting delay: $e');
      return false;
    }
  }

  // Set noise gate
  Future<bool> setNoiseGate({
    bool? enabled,
    double? threshold,
    double? attack,
    double? release,
  }) async {
    try {
      final data = <String, dynamic>{};
      if (enabled != null) data['enabled'] = enabled;
      if (threshold != null) data['threshold'] = threshold;
      if (attack != null) data['attack'] = attack;
      if (release != null) data['release'] = release;

      final response = await _dio.post(
        '$_baseUrl/api/gate',
        data: data,
      );
      return response.statusCode == 200;
    } catch (e) {
      print('Error setting noise gate: $e');
      return false;
    }
  }

  // Load preset
  Future<bool> loadPreset(String presetName) async {
    try {
      final response = await _dio.post('$_baseUrl/api/presets/$presetName');
      return response.statusCode == 200;
    } catch (e) {
      print('Error loading preset: $e');
      return false;
    }
  }

  // Get available presets
  Future<List<String>> getPresets() async {
    try {
      final response = await _dio.get('$_baseUrl/api/presets');
      if (response.statusCode == 200) {
        final presets = response.data['presets'] as List;
        return presets.map((p) => p['name'] as String).toList();
      }
      return [];
    } catch (e) {
      print('Error getting presets: $e');
      return [];
    }
  }
}
