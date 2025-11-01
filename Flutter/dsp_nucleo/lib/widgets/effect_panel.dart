import 'package:flutter/material.dart';

class EffectPanel extends StatelessWidget {
  final String title;
  final bool enabled;
  final VoidCallback onToggle;
  final List<Widget> children;
  final IconData icon;

  const EffectPanel({
    Key? key,
    required this.title,
    required this.enabled,
    required this.onToggle,
    required this.children,
    required this.icon,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 6, horizontal: 12),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: enabled
              ? [Colors.grey[900]!, Colors.grey[850]!]
              : [Colors.grey[900]!.withOpacity(0.5), Colors.grey[850]!.withOpacity(0.5)],
        ),
        borderRadius: BorderRadius.circular(16),
        boxShadow: [
          BoxShadow(
            color: enabled ? Colors.greenAccent.withOpacity(0.3) : Colors.black26,
            blurRadius: 12,
            spreadRadius: 1,
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Header
          Container(
            padding: const EdgeInsets.all(12),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  children: [
                    Icon(
                      icon,
                      color: enabled ? Colors.greenAccent : Colors.grey,
                      size: 24,
                    ),
                    const SizedBox(width: 10),
                    Text(
                      title,
                      style: TextStyle(
                        color: enabled ? Colors.white : Colors.grey,
                        fontSize: 19,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ],
                ),
                GestureDetector(
                  onTap: onToggle,
                  child: Container(
                    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                    decoration: BoxDecoration(
                      color: enabled ? Colors.greenAccent : Colors.redAccent,
                      borderRadius: BorderRadius.circular(16),
                      boxShadow: [
                        BoxShadow(
                          color: enabled
                              ? Colors.greenAccent.withOpacity(0.5)
                              : Colors.redAccent.withOpacity(0.5),
                          blurRadius: 8,
                          spreadRadius: 1,
                        ),
                      ],
                    ),
                    child: Text(
                      enabled ? 'ON' : 'OFF',
                      style: const TextStyle(
                        color: Colors.black,
                        fontSize: 13,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
          // Knobs
          if (enabled)
            Padding(
              padding: const EdgeInsets.only(left: 12, right: 12, bottom: 12),
              child: Wrap(
                spacing: 16,
                runSpacing: 16,
                alignment: WrapAlignment.spaceEvenly,
                children: children,
              ),
            ),
        ],
      ),
    );
  }
}
