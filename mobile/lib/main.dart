import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const BusBoardProvisioner());
}

class BusBoardProvisioner extends StatelessWidget {
  const BusBoardProvisioner({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'BusBoard BLE Setup',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: const DeviceListScreen(),
    );
  }
}

class DeviceListScreen extends StatefulWidget {
  const DeviceListScreen({super.key});

  @override
  State<DeviceListScreen> createState() => _DeviceListScreenState();
}

class _DeviceListScreenState extends State<DeviceListScreen> {
  List<BluetoothDevice> devices = [];

  @override
  void initState() {
    super.initState();
    requestPermissions();
  }

  Future<void> requestPermissions() async {
    await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();

    scanForDevices();
  }

  void scanForDevices() async {
    FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));

    FlutterBluePlus.scanResults.listen((results) {
      for (ScanResult r in results) {
        if (r.device.name.startsWith('Bus-Board-') && !devices.contains(r.device)) {
          setState(() {
            devices.add(r.device);
          });
        }
      }
    });

    await Future.delayed(const Duration(seconds: 5));
    FlutterBluePlus.stopScan();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Select BusBoard')),
      body: devices.isEmpty
          ? const Center(child: CircularProgressIndicator())
          : ListView.builder(
              itemCount: devices.length,
              itemBuilder: (context, index) {
                return ListTile(
                  title: Text(devices[index].name),
                  subtitle: Text(devices[index].id.id),
                  trailing: const Icon(Icons.bluetooth),
                  onTap: () {
                    Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (_) => ProvisionScreen(device: devices[index]),
                      ),
                    );
                  },
                );
              },
            ),
    );
  }
}

class ProvisionScreen extends StatefulWidget {
  final BluetoothDevice device;
  const ProvisionScreen({super.key, required this.device});

  @override
  State<ProvisionScreen> createState() => _ProvisionScreenState();
}

class _ProvisionScreenState extends State<ProvisionScreen> {
  final TextEditingController ssidController = TextEditingController();
  final TextEditingController passwordController = TextEditingController();
  bool connected = false;
  BluetoothCharacteristic? ssidChar;
  BluetoothCharacteristic? passChar;

  @override
  void initState() {
    super.initState();
    connectToDevice();
  }

  Future<void> connectToDevice() async {
    await widget.device.connect();
    setState(() {
      connected = true;
    });

    List<BluetoothService> services = await widget.device.discoverServices();
    for (var service in services) {
      if (service.uuid.toString() == "12345678-1234-1234-1234-1234567890ab") {
        for (var c in service.characteristics) {
          if (c.uuid.toString() == "abcd1234-1234-1234-1234-1234567890ab") ssidChar = c;
          if (c.uuid.toString() == "abcd5678-1234-1234-1234-1234567890ab") passChar = c;
        }
      }
    }
  }

  Future<void> sendCredentials() async {
    if (ssidChar != null && passChar != null) {
      await ssidChar!.write(ssidController.text.codeUnits);
      await passChar!.write(passwordController.text.codeUnits);
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Credentials sent!')));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(widget.device.name)),
      body: Padding(
        padding: const EdgeInsets.all(20.0),
        child: connected
            ? Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  TextField(
                    controller: ssidController,
                    decoration: const InputDecoration(
                      labelText: 'WiFi SSID',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 20),
                  TextField(
                    controller: passwordController,
                    decoration: const InputDecoration(
                      labelText: 'WiFi Password',
                      border: OutlineInputBorder(),
                    ),
                    obscureText: true,
                  ),
                  const SizedBox(height: 40),
                  ElevatedButton(
                    onPressed: sendCredentials,
                    child: const Text('Send to BusBoard'),
                  ),
                ],
              )
            : const Center(child: CircularProgressIndicator()),
      ),
    );
  }

  @override
  void dispose() {
    widget.device.disconnect();
    super.dispose();
  }
}

