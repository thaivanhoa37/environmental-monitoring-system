import 'package:flutter/material.dart';
import '../models/sensor_data.dart';
import '../services/firebase_service.dart';
import 'detail_screen.dart';

class HomeScreen extends StatelessWidget {
  final FirebaseService _firebaseService = FirebaseService();

  HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Environmental Monitor'),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
      ),
      body: StreamBuilder<List<SensorData>>(
        stream: _firebaseService.getSensorDataStream(),
        builder: (context, snapshot) {
          if (snapshot.hasError) {
            return Center(child: Text('Error: ${snapshot.error}'));
          }

          if (!snapshot.hasData) {
            return const Center(child: CircularProgressIndicator());
          }

          final latestData =
              snapshot.data!.isNotEmpty ? snapshot.data!.first : null;

          if (latestData == null) {
            return const Center(child: Text('No data available'));
          }

          return Padding(
            padding: const EdgeInsets.all(16.0),
            child: GridView.count(
              crossAxisCount: 2,
              mainAxisSpacing: 16.0,
              crossAxisSpacing: 16.0,
              children: [
                _buildSensorCard(
                  context,
                  'Temperature',
                  '${latestData.temperature.toStringAsFixed(1)}°C',
                  Icons.thermostat,
                  Colors.red,
                  () =>
                      _navigateToDetail(context, 'Temperature', snapshot.data!),
                ),
                _buildSensorCard(
                  context,
                  'Humidity',
                  '${latestData.humidity.toStringAsFixed(1)}%',
                  Icons.water_drop,
                  Colors.blue,
                  () => _navigateToDetail(context, 'Humidity', snapshot.data!),
                ),
                _buildSensorCard(
                  context,
                  'Light Intensity',
                  '${latestData.lightIntensity.toStringAsFixed(1)} lux',
                  Icons.light_mode,
                  Colors.amber,
                  () => _navigateToDetail(
                      context, 'Light Intensity', snapshot.data!),
                ),
                _buildSensorCard(
                  context,
                  'Soil Moisture',
                  '${latestData.soilMoisture.toStringAsFixed(1)}%',
                  Icons.grass,
                  Colors.green,
                  () => _navigateToDetail(
                      context, 'Soil Moisture', snapshot.data!),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  Widget _buildSensorCard(
    BuildContext context,
    String title,
    String value,
    IconData icon,
    Color color,
    VoidCallback onTap,
  ) {
    return Card(
      elevation: 4,
      child: InkWell(
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(16.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, size: 48, color: color),
              const SizedBox(height: 8),
              Text(
                title,
                style: const TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 4),
              Text(
                value,
                style: TextStyle(
                  fontSize: 20,
                  color: color,
                  fontWeight: FontWeight.w500,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  void _navigateToDetail(
      BuildContext context, String title, List<SensorData> data) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => DetailScreen(title: title, data: data),
      ),
    );
  }
}
