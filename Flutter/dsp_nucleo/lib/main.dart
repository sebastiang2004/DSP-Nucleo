import 'package:flutter/material.dart';
import 'package:dsp_nucleo/screens/home_screen.dart';

void main() {
  runApp(const DSPNucleoApp());
}

class DSPNucleoApp extends StatelessWidget {
  const DSPNucleoApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'STM32 DSP Guitar Pedal',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        primaryColor: const Color(0xFF4CAF50),
        scaffoldBackgroundColor: const Color(0xFF1a1a1a),
        colorScheme: ColorScheme.dark(
          primary: const Color(0xFF4CAF50),
          secondary: Colors.greenAccent,
          surface: Colors.grey[900]!,
        ),
        appBarTheme: AppBarTheme(
          backgroundColor: Colors.grey[900],
          elevation: 0,
        ),
        useMaterial3: true,
      ),
      home: const HomeScreen(),
    );
  }
}