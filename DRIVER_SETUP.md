# AudiDeck Driver Setup

## Quick Install

Open Terminal and run:

```bash
cd /Users/cv/go/src/github.com/Public/AudiDeck/AudiDeckDriver

# Build the driver (already done)
./build.sh

# Install (requires admin password)
sudo ./install.sh
```

## Verify Installation

After install, check if the driver is loaded:

```bash
system_profiler SPAudioDataType | grep -i audideck
```

You should see: **"AudiDeck Virtual Output"**

It will also appear in:
- **System Settings → Sound → Output**
- **AudiDeck app** as a device column

## Uninstall

```bash
cd /Users/cv/go/src/github.com/Public/AudiDeck/AudiDeckDriver
sudo ./uninstall.sh
```

## Troubleshooting

### Driver doesn't appear
1. Restart Core Audio manually:
   ```bash
   sudo killall coreaudiod
   ```
   (coreaudiod auto-restarts after being killed)

2. Check for errors:
   ```bash
   log show --last 5m --predicate 'process == "coreaudiod"' | grep -i audideck
   ```

### Driver needs to be re-installed after macOS update
HAL plugins need reinstallation after major macOS updates.

## For Distribution

When distributing AudiDeck to customers:

1. **Bundle the driver** in your app's Resources folder
2. **Use an installer package** (.pkg) that:
   - Copies the driver to `/Library/Audio/Plug-Ins/HAL/`
   - Sets correct permissions (root:wheel, 755)
   - Restarts coreaudiod

Or use a **post-install script** in your app that prompts for admin access and installs the driver.

### Example installer code (Swift):

```swift
func installDriver() {
    let script = """
        cp -R "\(driverPath)" /Library/Audio/Plug-Ins/HAL/
        chown -R root:wheel /Library/Audio/Plug-Ins/HAL/AudiDeckDriver.driver
        chmod -R 755 /Library/Audio/Plug-Ins/HAL/AudiDeckDriver.driver
        launchctl kickstart -kp system/com.apple.audio.coreaudiod
    """
    
    let appleScript = "do shell script \"\(script)\" with administrator privileges"
    NSAppleScript(source: appleScript)?.executeAndReturnError(nil)
}
```
