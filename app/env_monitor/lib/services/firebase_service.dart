import 'package:firebase_database/firebase_database.dart';
import '../models/sensor_data.dart';

class FirebaseService {
  final DatabaseReference _dbRef = FirebaseDatabase.instance.ref();

  Stream<List<SensorData>> getSensorDataStream() {
    return _dbRef.child('sensor_data').onValue.map((event) {
      final Map<dynamic, dynamic> data =
          event.snapshot.value as Map<dynamic, dynamic>;

      List<SensorData> sensorDataList = [];

      data.forEach((key, value) {
        if (value is Map<dynamic, dynamic>) {
          try {
            final sensorData =
                SensorData.fromMap(Map<String, dynamic>.from(value));
            sensorDataList.add(sensorData);
          } catch (e) {
            print('Error parsing sensor data: $e');
          }
        }
      });

      // Sort by timestamp, most recent first
      sensorDataList.sort((a, b) => b.timestamp.compareTo(a.timestamp));
      return sensorDataList;
    });
  }

  Future<List<SensorData>> getHistoricalData() async {
    final snapshot = await _dbRef
        .child('sensor_data')
        .limitToLast(100) // Last 100 readings
        .get();

    if (snapshot.value == null) return [];

    final Map<dynamic, dynamic> data = snapshot.value as Map<dynamic, dynamic>;

    List<SensorData> historicalData = [];

    data.forEach((key, value) {
      if (value is Map<dynamic, dynamic>) {
        try {
          final sensorData =
              SensorData.fromMap(Map<String, dynamic>.from(value));
          historicalData.add(sensorData);
        } catch (e) {
          print('Error parsing historical data: $e');
        }
      }
    });

    // Sort by timestamp
    historicalData.sort((a, b) => a.timestamp.compareTo(b.timestamp));
    return historicalData;
  }
}
