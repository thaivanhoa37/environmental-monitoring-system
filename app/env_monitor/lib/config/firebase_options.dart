import 'package:firebase_core/firebase_core.dart' show FirebaseOptions;
import 'package:flutter/foundation.dart'
    show defaultTargetPlatform, kIsWeb, TargetPlatform;

class DefaultFirebaseOptions {
  static FirebaseOptions get currentPlatform {
    if (kIsWeb) {
      throw UnsupportedError('Web platform not supported');
    }

    switch (defaultTargetPlatform) {
      case TargetPlatform.android:
        return android;
      case TargetPlatform.iOS:
        return ios;
      default:
        throw UnsupportedError(
          'DefaultFirebaseOptions are not supported for this platform.',
        );
    }
  }

  static const FirebaseOptions android = FirebaseOptions(
    apiKey: 'AIzaSyB3KKJNVxTdwAFsfn6YcqDX0Y2rn2FA1l8',
    appId: '1:104576759211:android:d8e9b5f5e0f3a7d5e2b890',
    messagingSenderId: '104576759211',
    projectId: 'environmental-monitoring-d50f8',
    databaseURL:
        'https://environmental-monitoring-d50f8-default-rtdb.asia-southeast1.firebasedatabase.app',
    storageBucket: 'environmental-monitoring-d50f8.appspot.com',
  );

  static const FirebaseOptions ios = FirebaseOptions(
    apiKey: 'AIzaSyDGa_xUEsWH6FPYvG6KmQB1xBOgJ7Q1ZjM',
    appId: '1:104576759211:ios:f9e8d4c6d1e2a3b4c5d6e7',
    messagingSenderId: '104576759211',
    projectId: 'environmental-monitoring-d50f8',
    databaseURL:
        'https://environmental-monitoring-d50f8-default-rtdb.asia-southeast1.firebasedatabase.app',
    storageBucket: 'environmental-monitoring-d50f8.appspot.com',
    iosClientId: 'your-ios-client-id.apps.googleusercontent.com',
  );
}
