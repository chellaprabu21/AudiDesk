# AudiDeck

**Route any app to any audio output on your Mac.**

AudiDeck is a macOS menu bar utility that lets you visually assign applications to different audio output devices using a Kanban-style interface.

![AudiDeck Preview](website/preview.png)

## Features

- 🎧 **Per-App Audio Routing** - Send Spotify to headphones, Slack to speakers
- 📋 **Kanban UI** - Drag and drop apps between audio devices
- 🖥️ **Menu Bar App** - Always accessible, one click away
- 🔊 **Virtual Audio Driver** - System-level audio routing

## Project Structure

```
AudiDeck/
├── AudiDeck/              # Main macOS app (SwiftUI)
│   ├── App/               # App entry point
│   ├── Views/             # Kanban UI components
│   ├── ViewModels/        # State management
│   └── Models/            # Data models
├── AudiDeckDriver/        # HAL Audio Plugin (C++)
│   ├── AudiDeckDriver.cpp # Virtual audio device
│   ├── build.sh           # Build script
│   └── install.sh         # Installation script
├── AudiDeckHelper/        # XPC Helper service
├── Shared/                # Shared protocols and types
└── website/               # Landing page for waitlist
```

## Requirements

- macOS 12.0+
- Xcode 14.0+
- Apple Developer ID (for driver signing)

## Building

### Main App

```bash
open AudiDeck.xcodeproj
# Build and run in Xcode
```

### Audio Driver

```bash
cd AudiDeckDriver
./build.sh
sudo ./install.sh
```

See [DRIVER_SETUP.md](DRIVER_SETUP.md) for detailed driver instructions.

## Website

The landing page is at `website/index.html`. See [website/DEPLOY.md](website/DEPLOY.md) for deployment instructions.

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for technical details.

## License

MIT License - See LICENSE file for details.

---

**AudiDeck** — Stop the audio chaos. Start routing.
