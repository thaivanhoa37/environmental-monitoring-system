class SensorData {
  final double temperature;
  final double humidity;
  final double lightIntensity;
  final double soilMoisture;
  final DateTime timestamp;

  SensorData({
    required this.temperature,
    required this.humidity,
    required this.lightIntensity,
    required this.soilMoisture,
    required this.timestamp,
  });

  factory SensorData.fromMap(Map<String, dynamic> map) {
    return SensorData(
      temperature: (map['temperature'] as num).toDouble(),
      humidity: (map['humidity'] as num).toDouble(),
      lightIntensity: (map['lightIntensity'] as num).toDouble(),
      soilMoisture: (map['soilMoisture'] as num).toDouble(),
      timestamp: DateTime.fromMillisecondsSinceEpoch(map['timestamp'] as int),
    );
  }
}
