class EffectState {
  double volume;
  OverdriveState overdrive;
  DelayState delay;
  NoiseGateState noiseGate;

  EffectState({
    this.volume = 0.7,
    OverdriveState? overdrive,
    DelayState? delay,
    NoiseGateState? noiseGate,
  })  : overdrive = overdrive ?? OverdriveState(),
        delay = delay ?? DelayState(),
        noiseGate = noiseGate ?? NoiseGateState();

  factory EffectState.fromJson(Map<String, dynamic> json) {
    return EffectState(
      volume: (json['volume'] ?? 0.7).toDouble(),
      overdrive: OverdriveState.fromJson(json['overdrive'] ?? {}),
      delay: DelayState.fromJson(json['delay'] ?? {}),
      noiseGate: json.containsKey('gate')
          ? NoiseGateState.fromJson(json['gate'])
          : NoiseGateState(),
    );
  }
}

class OverdriveState {
  bool enabled;
  double gain;
  double threshold;
  double tone;
  double mix;
  int mode;

  OverdriveState({
    this.enabled = false,
    this.gain = 5.0,
    this.threshold = 0.7,
    this.tone = 0.5,
    this.mix = 0.5,
    this.mode = 0,
  });

  factory OverdriveState.fromJson(Map<String, dynamic> json) {
    return OverdriveState(
      enabled: json['enabled'] ?? false,
      gain: (json['gain'] ?? 5.0).toDouble(),
      threshold: (json['threshold'] ?? 0.7).toDouble(),
      tone: (json['tone'] ?? 0.5).toDouble(),
      mix: (json['mix'] ?? 0.5).toDouble(),
      mode: json['mode'] ?? 0,
    );
  }
}

class DelayState {
  bool enabled;
  double timeMs;
  double feedback;
  double mix;
  double tone;

  DelayState({
    this.enabled = false,
    this.timeMs = 100.0,
    this.feedback = 0.3,
    this.mix = 0.3,
    this.tone = 0.5,
  });

  factory DelayState.fromJson(Map<String, dynamic> json) {
    return DelayState(
      enabled: json['enabled'] ?? false,
      timeMs: (json['time_ms'] ?? 100.0).toDouble(),
      feedback: (json['feedback'] ?? 0.3).toDouble(),
      mix: (json['mix'] ?? 0.3).toDouble(),
      tone: (json['tone'] ?? 0.5).toDouble(),
    );
  }
}

class NoiseGateState {
  bool enabled;
  double threshold;
  double attack;
  double release;

  NoiseGateState({
    this.enabled = true,
    this.threshold = 0.015,
    this.attack = 0.001,
    this.release = 0.15,
  });

  factory NoiseGateState.fromJson(Map<String, dynamic> json) {
    return NoiseGateState(
      enabled: json['enabled'] ?? true,
      threshold: (json['threshold'] ?? 0.015).toDouble(),
      attack: (json['attack'] ?? 0.001).toDouble(),
      release: (json['release'] ?? 0.15).toDouble(),
    );
  }
}
