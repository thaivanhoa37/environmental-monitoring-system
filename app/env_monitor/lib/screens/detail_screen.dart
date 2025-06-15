import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import '../models/sensor_data.dart';

class DetailScreen extends StatelessWidget {
  final String title;
  final List<SensorData> data;

  const DetailScreen({
    super.key,
    required this.title,
    required this.data,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(title),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            const Text(
              'Historical Data',
              style: TextStyle(
                fontSize: 20,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 16),
            Expanded(
              child: LineChart(
                LineChartData(
                  gridData: const FlGridData(show: true),
                  titlesData: FlTitlesData(
                    bottomTitles: AxisTitles(
                      sideTitles: SideTitles(
                        showTitles: true,
                        getTitlesWidget: (value, meta) {
                          if (value.toInt() >= 0 &&
                              value.toInt() < data.length) {
                            return RotatedBox(
                              quarterTurns: 1,
                              child: Text(
                                _formatDateTime(data[value.toInt()].timestamp),
                                style: const TextStyle(fontSize: 10),
                              ),
                            );
                          }
                          return const Text('');
                        },
                        reservedSize: 40,
                      ),
                    ),
                    leftTitles: AxisTitles(
                      sideTitles: SideTitles(
                        showTitles: true,
                        reservedSize: 40,
                      ),
                    ),
                    rightTitles: const AxisTitles(
                      sideTitles: SideTitles(showTitles: false),
                    ),
                    topTitles: const AxisTitles(
                      sideTitles: SideTitles(showTitles: false),
                    ),
                  ),
                  borderData: FlBorderData(show: true),
                  lineBarsData: [
                    LineChartBarData(
                      spots: _getDataPoints(),
                      isCurved: true,
                      color: _getColorForTitle(),
                      barWidth: 3,
                      dotData: const FlDotData(show: false),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  String _formatDateTime(DateTime dateTime) {
    return '${dateTime.hour}:${dateTime.minute.toString().padLeft(2, '0')}';
  }

  List<FlSpot> _getDataPoints() {
    return List.generate(data.length, (index) {
      final sensorData = data[index];
      double value = 0;

      switch (title) {
        case 'Temperature':
          value = sensorData.temperature;
          break;
        case 'Humidity':
          value = sensorData.humidity;
          break;
        case 'Light Intensity':
          value = sensorData.lightIntensity;
          break;
        case 'Soil Moisture':
          value = sensorData.soilMoisture;
          break;
      }

      return FlSpot(index.toDouble(), value);
    });
  }

  Color _getColorForTitle() {
    switch (title) {
      case 'Temperature':
        return Colors.red;
      case 'Humidity':
        return Colors.blue;
      case 'Light Intensity':
        return Colors.amber;
      case 'Soil Moisture':
        return Colors.green;
      default:
        return Colors.grey;
    }
  }
}
