Place your app icon here as `icon.png`.

Recommended source image:
- Format: PNG
- Size: at least 512×512 (1024×1024 recommended)

After adding `icon.png` run the VS Code task **Flutter: Generate App Icons** or:

```bash
cd mobile
flutter pub get
flutter pub run flutter_launcher_icons:main
```

Then build the APK:

```bash
flutter build apk --debug
adb install -r build/app/outputs/flutter-apk/app-debug.apk
```

Important: Launcher masking and refreshing the icon

- Modern Android launchers (including Pixel) use *adaptive icons* and will apply a shape mask (commonly circular). If your icon fills the full square, the launcher may crop it to a circle — to get the best result, design your image so important content is centered and does not rely on the corners. If you want the entire image to appear, consider adding a background that matches the edges of your artwork.
- If you previously installed the app, the launcher may cache the old icon. To force an update on a connected Pixel:
  1. Uninstall the app from the device: `adb uninstall <applicationId>` (applicationId is defined in `android/app/src/main/AndroidManifest.xml` or `android/app/build.gradle`).
  2. Install the new APK: `adb install -r build/app/outputs/flutter-apk/app-debug.apk`.
  3. If the icon still doesn't update, restart the launcher or reboot the phone.

If you want me to generate the icons now, upload the PNG or tell me the file path and I'll run the generator and verify the APK includes it.