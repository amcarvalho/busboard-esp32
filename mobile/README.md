# bus_board_provisioner

A new Flutter project.

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Lab: Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Cookbook: Useful Flutter samples](https://docs.flutter.dev/cookbook)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.

## App icon (Android & iOS) 🔖

Place your app icon PNG at `mobile/assets/icon.png` (create the `assets/` directory if it doesn't exist). Recommended source image:
- Format: PNG
- Size: at least 512×512; 1024×1024 is recommended for best results
- Transparency is fine (foreground image) — avoid placing background color unless desired for adaptive icons

Then generate platform icons with the included dev tool:

```bash
cd mobile
flutter pub get
flutter pub run flutter_launcher_icons:main
```

This will populate `android/app/src/main/res/` and the iOS asset catalogs with properly sized icons. After that, build your APK as usual (e.g., `flutter build apk --debug` or use the VS Code task `Flutter: Generate App Icons`).

If you prefer, you can also run the VS Code task: **Tasks: Run Task → Flutter: Generate App Icons**

Note: If you later change the icon file, re-run the generator to update platform resources.
