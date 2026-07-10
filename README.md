# CBF - Click Beyond Frames

[![Geode](https://img.shields.io/badge/Geode-v1.8.0-blue)](https://geode-sdk.org)
[![GD](https://img.shields.io/badge/GD-2.208-brightgreen)](https://geometrydash.com)
[![Platform](https://img.shields.io/badge/Platform-Android-green)](https://developer.android.com)
[![Version](https://img.shields.io/badge/Version-v1.0.0-orange)](#)

**Click Beyond Frames** removes the **480 TPS cap** from RobTop's official "Click Between Steps" feature, allowing for **unlimited input precision**.

---

## Features

- **Unlimited Polling Rate** – Set custom TPS up to 2000 Hz.
- **Smooth Input** – Processes clicks independently from the frame rate.
- **Lightweight** – Minimal CPU overhead (~1ms sleep cycle).
- **API Integration** – Other mods can query your CBF state.
- **Vanilla+** – Built on RobTop's official implementation.

---

## Important Notice

- This mod **exceeds** the official 480 TPS limit set by RobTop.
- Records set **above 480 Hz** may **not be accepted** on all demon lists (Platformer list explicitly bans CBF).
- Use **at your own discretion**.

> A one-time warning popup will appear the first time you load a level.

---

## Requirements

| Requirement | Version |
| :--- | :--- |
| **Geometry Dash** | 2.208 |
| **Geode Loader** | v1.8.0 |
| **Platform** | Android (ARM 32bit & 64bit)

---

## Building from Source

### 1. Clone the repository

```bash
git clone https://github.com/frostre1997/Click-Beyond-Frames.git
cd Click-Beyond-Frames
```

### 2. Install the API dependency

## Option A: Install from Geode mod list
geode install frostre1997.Click-Beyond-Frames-API

## Option B: Use Git submodule
- git submodule add https://github.com/frostre1997/Click-Beyond-Frames-API.git

- git submodule update --init --recursive

## 3. Build for Android

```bash
// android64-bit
geode build --target android --abi arm64-v8a

// android32-bit
geode build --target android --abi arm64-v7a
```

## 4. Install on your device

```## Using ADB
// android64-bit
adb install -r build/android/arm64-v8a/ClickBeyondFrames.geode

// android32-bit
adb install -r build/android/arm64-v7a/ClickBeyondFrames.geode

// Or drag the .geode file into your GD folder on the tablet
```

---

# API for Developers

This Mod provides a public API for other mods to interact:

## Check if Click Beyond Frames is installed

```API.hpp
#include <ClickBeyondFrames/API.hpp>
if (cbfbeyond::dev::isInstalled()) {
    // Mod is present
}
```

---

## Get runtime state (anti-cheat)

```Checker.hpp
#include <ClickBeyondFrames/Checker.hpp>
auto state = clickbeyondframes::checker::getState();
if (state.isExceedingVanilla) {
    // User is above 480 TPS
}
```

---

# Anti-Cheat Intagration

The checker API exposes:

- Current polling rate
- Whether it exceeds 480 TPS
- Thread health status
- Bot override state
- Cryptographic signature for verification

---

# License

MIT – Use freely.

---

# Credits

- RobTop – Official CBF implementation.
- Geode SDK Team – Modding framework.
- Pointercrate – CBF legitimacy clarification.

---


# Support

If you want to contribute or help this repository:

- Github Issues
- Fork the repository
- Discord Server (coming soon)

---

Push your skill beyond the frames 🤍
