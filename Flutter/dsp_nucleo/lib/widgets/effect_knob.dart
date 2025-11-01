import 'package:flutter/material.dart';
import 'package:flutter_dial_knob/flutter_dial_knob.dart';

class EffectKnob extends StatelessWidget {
  final String label;
  final double value;
  final double min;
  final double max;
  final Function(double) onChanged;
  final String? unit;
  final int decimals;
  final Color? knobColor;
  final Color? levelColorStart;
  final Color? levelColorEnd;

  const EffectKnob({
    Key? key,
    required this.label,
    required this.value,
    required this.min,
    required this.max,
    required this.onChanged,
    this.unit,
    this.decimals = 2,
    this.knobColor,
    this.levelColorStart,
    this.levelColorEnd,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    // Get screen width for responsive sizing
    final screenWidth = MediaQuery.of(context).size.width;
    // Calculate knob size based on screen width (iPhone 11: 414pt)
    final knobSize = screenWidth > 400 ? 75.0 : 65.0;
    
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          label,
          style: const TextStyle(
            color: Colors.white70,
            fontSize: 11,
            fontWeight: FontWeight.w500,
          ),
          textAlign: TextAlign.center,
        ),
        const SizedBox(height: 6),
        DialKnob(
          value: value,
          onChanged: onChanged,
          min: min,
          max: max,
          size: knobSize,
          trackColor: Colors.grey[800]!,
          levelColorStart: levelColorStart ?? Colors.greenAccent,
          levelColorEnd: levelColorEnd ?? Colors.lightGreen,
          knobColor: knobColor ?? const Color(0xFF4CAF50),
          indicatorColor: Colors.white,
        ),
        const SizedBox(height: 6),
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
          decoration: BoxDecoration(
            color: Colors.black54,
            borderRadius: BorderRadius.circular(8),
          ),
          child: Text(
            '${value.toStringAsFixed(decimals)}${unit ?? ''}',
            style: const TextStyle(
              color: Colors.greenAccent,
              fontSize: 12,
              fontWeight: FontWeight.bold,
            ),
          ),
        ),
      ],
    );
  }
}
