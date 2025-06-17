import 'package:firebase_database/firebase_database.dart';

class FirebaseService {
  final DatabaseReference _dbRef =
      FirebaseDatabase.instance.ref('env_data/sensors');

  Stream<DatabaseEvent> monitorEnvironmentalData() {
    return _dbRef.onValue;
  }

  Future<Map<String, dynamic>?> getCurrentData() async {
    try {
      final snapshot = await _dbRef.get();
      if (snapshot.exists) {
        return Map<String, dynamic>.from(snapshot.value as Map);
      }
      return null;
    } catch (e) {
      print('Error getting current data: $e');
      return null;
    }
  }
}
