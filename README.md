# 🔓 Unlocker Pro

> A powerful Windows utility to unlock, delete, and take control of files that Windows refuses to touch.

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/rahulkanasiya/unlocker-pro/blob/main/unlocker.exe)
[![Platform](https://img.shields.io/badge/platform-Windows%207%2F8%2F10%2F11-lightgrey.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C-blue.svg)]()

---

## 📖 Overview

**Unlocker Pro** is a lightweight, native Windows tool written in pure C that helps you:

- **Unlock** files held by other processes
- **Force delete** stubborn files that Windows won't remove
- **Grant full permissions** (777) to any file or folder
- **Take ownership** of files locked by another user/account
- **Identify** which processes are locking a file

It integrates directly into **Windows Explorer's right-click context menu** — no separate GUI, no bloat, no dependencies.

---

## ✨ Features

| Feature | Description |
|---|---|
| 🗑️ **Force Delete** | 6-stage fallback: attribute fix → permission grant → Restart Manager → handle close → ownership → reboot-schedule |
| 🔓 **Unlock** | Uses Windows Restart Manager API + aggressive handle enumeration |
| 🛡️ **Grant Everyone (777)** | Full access via ACL manipulation |
| 🔑 **Take Ownership** | Transfers NTFS ownership to current user |
| 🔍 **Show Locking Apps** | Lists every process currently holding the file |
| ⚠️ **Reboot Schedule** | If file can't be deleted now, schedules it for next reboot |
| 🔔 **Popup Alerts** | Success/failure notifications for every operation |
| 🔇 **Silent Mode** | `-silent` flag suppresses popups — perfect for scripting/plugins |
| 🎨 **Native Icons** | Integrates with Windows shell icons (no bloat) |
| ⚡ **Zero Dependencies** | Single `.exe` — no DLLs, no runtime, no installer framework |
| 🖥️ **CLI + GUI** | Works from Explorer right-click **and** command line |
| 🔧 **Auto-Repair** | Fixes broken installations automatically |
| 📋 **Control Panel Entry** | Standard Windows uninstall integration |

---

## 📸 Screenshots

<img width="1041" height="800" alt="image" src="https://github.com/user-attachments/assets/cad01085-b9a4-4d60-81cf-ed4727abbe17" />

<img width="420" height="229" alt="image" src="https://github.com/user-attachments/assets/90aa1ea6-8e23-4ffe-b3b4-4a220141a8b3" />

<img width="984" height="579" alt="image" src="https://github.com/user-attachments/assets/a0d8761f-eaa9-4105-8d57-305f601fe500" />


---

## 🚀 Installation

### Option 1: Pre-built Binary (Recommended)

1. Go to the [**Releases**](https://github.com/yourusername/unlocker-pro/releases) page
2. Download the latest `unlocker.exe`
3. **Right-click** → **Run as Administrator**
4. When prompted, type `Y` and press Enter

The installer will:
- Copy `unlocker.exe` to `C:\Program Files\UnlockerPro\`
- Register context menu entries (files, folders, drives, desktop)
- Add an entry to Control Panel → Programs and Features

### Option 2: Build from Source

**Requirements:**
- MinGW-w64 (via MSYS2, or standalone)
- `windres` (bundled with MinGW)
- `icon.ico` file (256×256 recommended)

**Build steps:**

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/unlocker-pro.git
cd unlocker-pro

# 2. Compile the resource file (embeds .exe icon)
windres app.rc -O coff -o app.res

# 3. Compile the main program
gcc unlocker.c app.res -o unlocker.exe \
    -mwindows -static -static-libgcc \
    -lcomctl32 -lshell32 -lole32 -ladvapi32 -lrstrtmgr -luser32 -lgdi32
```

**For MSVC users:**

```cmd
rc app.rc
cl unlocker.c app.res /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib ^
   ole32.lib advapi32.lib rstrtmgr.lib comctl32.lib gdi32.lib
```

---

## 📚 Usage

### 🖱️ GUI Mode (Right-Click Menu)

1. **Right-click** any file, folder, or drive
   (For windows 10 or later ) **Shift + Right-click** any file, folder, or drive
2. Select **"Unlocker Pro"** → submenu appears
3. Choose an operation:

| Menu Item | Action |
|---|---|
| **Force Delete** | Permanently delete the file, bypassing all locks |
| **Unlock (release lock)** | Release any process holding the file |
| **Grant Everyone (777)** | Give full permissions to all users |
| **Take Ownership** | Change file owner to you |
| **Show locking apps** | See which processes are locking the file |

4. A **popup** will show the result of the operation

### 💻 Command Line Mode

```bash
unlocker.exe [options]
```

#### Management Commands

| Command | Description |
|---|---|
| `unlocker.exe` | Interactive mode (menu-driven) |
| `unlocker.exe -install` | Install to `C:\Program Files\UnlockerPro\` |
| `unlocker.exe -uninstall` | Remove all registry entries and files |
| `unlocker.exe -repair` | Auto-detect and fix broken installations |
| `unlocker.exe -status` | Show current installation state |

#### File Operations

```bash
unlocker.exe -f "<file>" <COMMAND> [options]
```

| Command | Description |
|---|---|
| `P` | Show locking processes (popup, or JSON with `-silent`) |
| `U` | Unlock the file |
| `D` | Force delete the file |
| `A` | Grant Everyone full access (777) |
| `O` | Take ownership |
| `R <newname>` | Rename the file |
| `M <directory>` | Move the file to a directory |
| `C <directory>` | Copy the file to a directory |

#### Options

| Option | Description |
|---|---|
| `-silent` | Suppress all popups — output goes to console only |

### 📝 Examples

**Force delete a locked file:**
```bash
unlocker.exe -f "C:\temp\stubborn.dll" D
```

**Unlock a file silently (for plugins/scripts):**
```bash
unlocker.exe -f "C:\data\locked.txt" U -silent
```

**Get locking processes as JSON:**
```bash
unlocker.exe -f "C:\video.mp4" P -silent
```

**Output:**
```json
{
  "file": "C:\\video.mp4",
  "locking_count": 2,
  "locking_processes": [
    {
      "pid": 12345,
      "process": "vlc.exe",
      "application": "VLC media player"
    },
    {
      "pid": 6789,
      "process": "explorer.exe",
      "application": "Windows Explorer"
    }
  ]
}
```

**Batch script example:**
```batch
@echo off
for %%f in (C:\cleanup\*.tmp) do (
    unlocker.exe -f "%%f" D -silent
)
```

---

## 🔄 How It Works

### Force Delete — 6 Stages

1. **Fix attributes** — Removes `READONLY`, `HIDDEN`, `SYSTEM` flags
2. **Grant Everyone** — Modifies ACL to give full access
3. **Restart Manager unlock** — Uses Windows RM API to release locks
4. **Handle enumeration** — Scans ALL processes for handles to the file and force-closes them
5. **Take ownership** — Changes NTFS owner, re-grants access
6. **Schedule on reboot** — Uses `PendingFileRenameOperations` for reboot-time deletion

Each stage tries to delete the file. If one fails, the next kicks in.

### Unlock

Uses two approaches:
- **Restart Manager API** (`RmStartSession`, `RmShutdown`) — the official way
- **Handle enumeration** (`NtQuerySystemInformation`) — the aggressive way

### Elevation

Operations requiring admin rights automatically re-launch the process with UAC elevation via `ShellExecuteEx` with `runas` verb.

---

## ⚙️ Technical Details

| Aspect             |     Detail                           |
|--------------------|--------------------------------------|
| **Language**       | C (C89-compatible)                   |
| **Lines of Code**  | 2068                                 |
| **Binary Size**    | ~145 KB (static)                     |
| **Dependencies**   | None (uses Windows API only)         |
| **Minimum Windows**| Windows 7 (Vista should work)        |
| **Architecture**   | x86_64 (32-bit build possible)       |
| **Compiler**       | MinGW-w64 GCC 13+, MSVC 2019+        |
| **Subsystem**      | Windows GUI (`-mwindows`)            |
| **Icon Source**    | `shell32.dll` (native Windows icons) |

### APIs Used

- `Restart Manager` (`rstrtmgr.dll`) — file lock detection/release
- `NT Native API` (`ntdll.dll`) — system handle enumeration
- `Security API` (`advapi32.dll`) — ownership and ACL manipulation
- `Shell API` (`shell32.dll`) — elevation and icon loading
- `Registry API` (`advapi32.dll`) — context menu registration

---

## 🔒 Security & Privacy

- ✅ **100% offline** — no network activity whatsoever
- ✅ **No telemetry** — no data collection
- ✅ **Open source** — audit the code yourself
- ✅ **No auto-update** — you control when to update
- ✅ **Standard Windows APIs only** — no kernel patches, no drivers

**⚠️ Important:** This tool requires **administrator privileges** because it manipulates file permissions, ownership, and system handles. This is standard for utilities of this type (similar to Sysinternals `handle.exe`).

---

## 🐛 Troubleshooting

### Context menu not appearing

```bash
unlocker.exe -repair
ie4uinit.exe -show
```

Or restart Windows Explorer from Task Manager.

### Icon not showing correctly

Windows caches icons aggressively. Refresh the cache:

```bash
ie4uinit.exe -show
```

### File still locked after "Unlock"

Some files are held by kernel-mode drivers or system processes that can't be safely detached. In such cases:

- Use **Force Delete** → it will fall back to reboot-time deletion
- Restart Windows to complete the operation

### "Elevation failed" error

- Right-click `unlocker.exe` → **Run as Administrator** manually
- Or ensure UAC is not disabled by group policy

### Antivirus flags it

Some antivirus software flags any tool that manipulates file handles (same reason Sysinternals tools are flagged). Add an exception for `unlocker.exe`.

The full source code is available here — audit it, build it yourself, and verify.

-------------------------------------------------------------------------------------------

## 🗑️ Uninstallation

### Method 1: Control Panel

1. Open **Control Panel** → **Programs and Features**
2. Find **Unlocker Pro**
3. Click **Uninstall**

### Method 2: Command Line

```bash
"C:\Program Files\UnlockerPro\unlocker.exe" -uninstall
```

### Method 3: Manual Cleanup

If the above fails:

```bash 
# Delete registry entries


reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\UnlockerPro" /f
reg delete "HKLM\SOFTWARE\Classes\*\shell\UnlockerPro" /f
reg delete "HKLM\SOFTWARE\Classes\Directory\shell\UnlockerPro" /f
reg delete "HKLM\SOFTWARE\Classes\Drive\shell\UnlockerPro" /f
reg delete "HKLM\SOFTWARE\Classes\Directory\Background\shell\UnlockerPro" /f
reg delete "HKLM\SOFTWARE\Classes\UnlockerProMenu" /f

# Delete install folder

rmdir /s /q "C:\Program Files\UnlockerPro"
```


---------------------------------------------------------------------------------------

## 🛠️ Building & Development

### Project Structure

````````````````````````````````````````````````````
unlocker-pro/                                     ``
├── unlocker.c        # Main source code          ``
├── app.rc            # Resource script (icon)    ``
├── icon.ico          # Application icon          ``
├── README.md         # This file                 ``
└── LICENSE           # MIT License               ``
````````````````````````````````````````````````````

### Building

```bash
# Debug build (with console for testing)
gcc unlocker.c app.res -o unlocker-debug.exe \
    -lcomctl32 -lshell32 -lole32 -ladvapi32 -lrstrtmgr -luser32 -lgdi32

# Release build (no console, static)
gcc unlocker.c app.res -o unlocker.exe \
    -mwindows -static -static-libgcc -O2 \
    -lcomctl32 -lshell32 -lole32 -ladvapi32 -lrstrtmgr -luser32 -lgdi32
```

### Contributing

Contributions are welcome! Here's how:

1. **Fork** the repository
2. **Create a branch** (`git checkout -b feature/amazing-feature`)
3. **Commit changes** (`git commit -m 'Add amazing feature'`)
4. **Push** (`git push origin feature/amazing-feature`)
5. **Open a Pull Request**

### Development Guidelines

- **Keep it native** — pure Win32 API, no external libraries
- **No new dependencies** — if you must add one, discuss in an issue first
- **Test on Windows 10 + 11** before submitting
- **Preserve the single-file structure** — `unlocker.c` + `app.rc` only
- **Follow existing code style** — 4-space indent, K&R-ish braces

### Areas That Need Improvement

We'd love help with:

- 🌐 **Multi-language support** (Hindi, Spanish, Chinese, etc.)
- 🖱️ **Multi-file selection** in context menu
- 📊 **Logging system** (verbose mode)
- 🎨 **Config file** (`unlocker.ini`)
- 🔄 **Auto-update mechanism**
- 📦 **MSI installer** (WiX Toolset)
- 🧪 **Unit tests** for core functions
- 📱 **ARM64 build support**

-------------------------------------------------------------------------

## ❓ FAQ

**Q: Is this safe to use?**  
A: Yes. It uses only documented Windows APIs. However, like any tool that manipulates files, use it responsibly. Read the code — it's open source.

**Q: Will it work on Windows XP?**  
A: No. Requires Windows 7+ (Restart Manager API requires Vista+).

**Q: Does it need .NET or Visual C++ Runtime?**  
A: No. Static build means zero dependencies.

**Q: Can I use it in my own software as a plugin?**  
A: Absolutely! Use the `-silent` flag for automation:
```bash
unlocker.exe -f "file" D -silent
```
Check the exit code (0 = success, 1 = failure) and read stdout for messages.

**Q: Why does it ask for admin rights?**  
A: To modify file ownership, permissions, and handle system-level file locks.

**Q: Can it recover deleted files?**  
A: No. It only helps delete/unlock files.

**Q: Does it work on network drives?**  
A: Partially. Local drives work fully. Network shares have limitations (handle enumeration may not work).

**Q: Can I disable specific menu items?**  
A: Not yet. Edit the `subs[]` array in `unlocker.c` to customize.

-----------------------------------------------------------------------------------------------------------------


## 🌟 Show Your Support

If this tool helped you, please:

- ⭐ **Star** this repository
- 🐛 **Report bugs** via [Issues](https://github.com/rahulkanasiya/unlocker-pro/issues)
- 💡 **Suggest features** via [Discussions](https://github.com/rahulkanasiya/unlocker-pro/discussions)
- 🔀 **Submit pull requests**
- 📢 **Share** with others who might find it useful

---------------------------------------------------------------------------------------------------------------

## 📬 Contact

- **Author**: [Rahul Kanasiya Alex]
- **GitHub**: [@rahulkanasiya](https://github.com/rahulkanasiya)
- **Email**: rahul.kanasiya@gmail.com

-------------------------------------------------------------------------------------------------------------

## ⚠️ Disclaimer

This software is provided as-is. The author is **not responsible** for any data loss, system damage, or other consequences resulting from its use. Always **back up important files** before using force-delete or permission-modifying operations.

**Use at your own risk.**

---------------------------------------------------------------------------------------------------------------

<div align="center">

**Made with ❤️ for the Windows community**

⭐ **Don't forget to star this repo if you find it useful!** ⭐

</div>
