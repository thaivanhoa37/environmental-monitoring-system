import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:env_monitor/services/firebase_service.dart';
import 'package:env_monitor/config/firebase_options.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(
    options: DefaultFirebaseOptions.currentPlatform,
  );
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Environmental Monitor',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.green),
        useMaterial3: true,
      ),
      home: const MonitoringPage(),
    );
  }
}

class MonitoringPage extends StatefulWidget {
  const MonitoringPage({super.key});

  @override
  State<MonitoringPage> createState() => _MonitoringPageState();
}

class _MonitoringPageState extends State<MonitoringPage> {
  final FirebaseService _firebaseService = FirebaseService();
  Map<String, dynamic>? _currentData;

  @override
  void initState() {
    super.initState();
    _setupRealtimeUpdates();
  }

  void _setupRealtimeUpdates() {
    _firebaseService.monitorEnvironmentalData().listen((DatabaseEvent event) {
      if (event.snapshot.exists) {
        final data = Map<String, dynamic>.from(event.snapshot.value as Map);
        setState(() {
          _currentData = data;
        });
      }
    });
  }

  Widget _buildDataCard(
      String title, String value, IconData icon, Color color) {
    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Icon(icon, size: 40, color: color),
            const SizedBox(height: 8),
            Text(
              title,
              style: const TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              value,
              style: TextStyle(
                fontSize: 24,
                fontWeight: FontWeight.bold,
                color: color,
              ),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Environmental Monitor'),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
      ),
      body: _currentData == null
          ? const Center(child: CircularProgressIndicator())
          : RefreshIndicator(
              onRefresh: () async {
                final data = await _firebaseService.getCurrentData();
                if (data != null) {
                  setState(() {
                    _currentData = data;
                  });
                }
              },
              child: SingleChildScrollView(
                physics: const AlwaysScrollableScrollPhysics(),
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  children: [
                    GridView.count(
                      shrinkWrap: true,
                      physics: const NeverScrollableScrollPhysics(),
                      crossAxisCount: 2,
                      crossAxisSpacing: 10,
                      mainAxisSpacing: 10,
                      children: [
                        _buildDataCard(
                          'Temperature',
                          '${_currentData!['temperature'].toStringAsFixed(1)}°C',
                          Icons.thermostat,
                          Colors.red,
                        ),
                        _buildDataCard(
                          'Humidity',
                          '${_currentData!['humidity'].toStringAsFixed(1)}%',
                          Icons.water_drop,
                          Colors.blue,
                        ),
                        _buildDataCard(
                          'CO2',
                          '${_currentData!['co2'].toStringAsFixed(1)} ppm',
                          Icons.cloud,
                          Colors.purple,
                        ),
                        _buildDataCard(
                          'Dust',
                          '${_currentData!['dust'].toStringAsFixed(1)} μg/m³',
                          Icons.blur_on,
                          Colors.brown,
                        ),
                        _buildDataCard(
                          'Pressure',
                          '${_currentData!['pressure'].toStringAsFixed(1)} hPa',
                          Icons.compress,
                          Colors.teal,
                        ),
                        _buildDataCard(
                          'AQI',
                          _currentData!['aqi'].toString(),
                          Icons.air,
                          Colors.green,
                        ),
                      ],
                    ),
                    const SizedBox(height: 20),
                    Card(
                      child: Padding(
                        padding: const EdgeInsets.all(16.0),
                        child: Column(
                          children: [
                            const Text(
                              'Last Updated',
                              style: TextStyle(
                                fontSize: 16,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            const SizedBox(height: 8),
                            Text(
                              DateTime.now().toString(),
                              style: const TextStyle(fontSize: 14),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
    );
  }
}
