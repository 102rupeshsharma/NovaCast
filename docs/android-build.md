# Building NovaCast for Android

The Android app lives in `mobile/`. It is a normal Gradle project (Kotlin,
Jetpack Compose) whose native part builds the shared engine in `core/` with
the Android NDK and exposes it through a thin JNI bridge
(`mobile/app/src/main/cpp/novacast_jni.cpp`). No emulation code is duplicated.

## Requirements

| Tool | Version used and verified |
|------|---------------------------|
| JDK | 17 (Temurin 17.0.21) |
| Android SDK platform | 35 (Android 15) |
| Build tools | 35.0.0 |
| NDK | 27.2.12479018 (r27c) |
| CMake (SDK package) | 3.22.1 |
| Gradle | 8.10.2 (wrapper included: `mobile/gradlew`) |
| Android Gradle Plugin / Kotlin | 8.7.3 / 2.0.21 |

Android Studio (Ladybug or newer) can open `mobile/` directly and installs
the SDK packages for you. Without Android Studio, install the command-line
tools and run:

```bash
sdkmanager "platforms;android-35" "build-tools;35.0.0" "ndk;27.2.12479018" "cmake;3.22.1"
```

Point Gradle at the SDK with `ANDROID_HOME` or with `mobile/local.properties`
(git-ignored). The file uses forward slashes and escapes the drive colon:

```properties
sdk.dir=C\:/Users/you/AppData/Local/Android/Sdk
```

## Build

```bash
cd mobile
./gradlew assembleDebug        # app/build/outputs/apk/debug/app-debug.apk
./gradlew assembleRelease      # app/build/outputs/apk/release/app-release.apk
./gradlew connectedDebugAndroidTest   # instrumented tests on a device or emulator
```

On Windows use `gradlew.bat`. The APK contains the engine for `arm64-v8a`
(phones and tablets) and `x86_64` (the Android emulator). 32-bit ARM is not
built.

The app version comes from the repository's `VERSION` file, the same one the
desktop build and the native engine use.

## Release signing

Signing values are read from environment variables (used by CI) or from
`mobile/keystore.properties` (local, git-ignored):

| keystore.properties | Environment variable |
|---------------------|----------------------|
| `storeFile` | `NOVACAST_KEYSTORE_FILE` |
| `storePassword` | `NOVACAST_KEYSTORE_PASSWORD` |
| `keyAlias` | `NOVACAST_KEY_ALIAS` |
| `keyPassword` | `NOVACAST_KEY_PASSWORD` |

Without them, `assembleRelease` produces an unsigned APK that Android will
not install. Create a key once and keep it safe: every future update must be
signed with the same key.

```bash
keytool -genkeypair -keystore novacast-release.jks -storetype PKCS12 \
        -alias novacast -keyalg RSA -keysize 4096 -validity 10000
```

Never commit the keystore or its passwords. `mobile/.gitignore` excludes
`keystore.properties`, `*.jks` and `*.keystore`.

For GitHub Actions, add these repository secrets (Settings → Secrets and
variables → Actions):

| Secret | Value |
|--------|-------|
| `ANDROID_KEYSTORE_BASE64` | output of `base64 -w0 novacast-release.jks` |
| `ANDROID_KEYSTORE_PASSWORD` | store password |
| `ANDROID_KEY_ALIAS` | `novacast` (or your alias) |
| `ANDROID_KEY_PASSWORD` | key password |

The release workflow refuses to publish if these are missing.

## How the app uses the engine

- `NativeBridge.kt` declares the JNI functions; `novacast_jni.cpp` implements
  them by calling `nc::Emulator` from `core/system`.
- `EmulatorEngine.kt` confines every native call to one dedicated thread, so
  the C++ engine is never used from two threads at once.
- The engine's log goes to logcat (tag `NovaCast`) through a sink installed in
  `JNI_OnLoad`.
- C++ exceptions are turned into Java `RuntimeException`s at the bridge; they
  never cross into the Java runtime unhandled.
- Files are opened with the Storage Access Framework, so the app requests no
  storage permission. A chosen boot ROM is copied into app-private storage so
  it is remembered between launches.
- Emulation pauses when the app leaves the foreground and when Android
  reports severe thermal status (Android 10+).

## What the Android app cannot do yet

Exactly the same as the desktop app, because they share the engine: no video,
audio, disc images, controllers, VMU or save states. The engine has no GPU,
AICA, GD-ROM or Maple emulation yet. Touch controls and gamepad support will
be added when there is an emulated controller to connect them to. There is no
JIT; the SH-4 runs on the shared interpreter, which is several times slower
than real time.
