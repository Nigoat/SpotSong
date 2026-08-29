# SpotSong ---- Terminal Music player

A native cross-platform terminal music player written in modern C++17 and Qt 6. SpotSong is a 100% local-only audio library manager and player that runs directly in your terminal with rich purple ANSI styling, full keyboard navigation, and Discord Rich Presence.

---

## 1. Features

- **Terminal User Interface (TUI)**: Beautiful purple ANSI terminal interface with full keyboard controls and no mouse required.
- **Discord Rich Presence**: Live activity status on your Discord profile with current song, artist, duration progress bar, and play/pause status.
- **Local SQLite Database**: All songs, playlists, play counts, and settings are saved locally in SQLite.
- **Metadata Extraction**: Automatic ID3 / TagLib tag extraction for Title, Artist, Album, and Duration.
- **Playlists & Queue**: Create playlists, add/remove tracks, reorder, shuffle, and repeat.

---

## 2. Keyboard Controls

| Key | Action |
|---|---|
| **Space** | Play / Pause playback |
| **↑ / ↓** or **k / j** | Move cursor up / down in active list |
| **Enter** | Play selected song / Open selected playlist / Confirm action |
| **Tab** / **Shift+Tab** | Next / Previous view tab |
| **1 - 5** | Jump directly to tab (1: Home, 2: Library, 3: Playlists, 4: Queue, 5: Search) |
| **n / p** | Next track / Previous track |
| **Left / Right** or **[ / ]** | Seek backward / forward 5 seconds |
| **+ / -** or **= / -** | Volume up / Volume down |
| **m** | Toggle mute |
| **s** | Toggle shuffle mode |
| **r** | Cycle repeat mode (Off -> Repeat All -> Repeat One) |
| **/** | Open interactive search prompt |
| **i** | Open interactive import prompt (file path or directory) |
| **c** | Create new playlist prompt |
| **a** | Add selected song to a playlist |
| **d** | Delete song or playlist |
| **Esc** | Cancel prompt / Return to list |
| **q** | Quit SpotSong |

---

## 3. How to Compile

### Debian / Ubuntu (Linux)

#### Step 1: Install Required Packages
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    qt6-base-dev \
    qt6-multimedia-dev \
    libqt6sql6-sqlite \
    libtag1-dev
```

#### Step 2: Configure and Build
```bash
cd ~/Documents/SpotSong
mkdir -p build-linux && cd build-linux

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

#### Step 3: Run SpotSong
```bash
./spotsong
```

---

## 4. License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See the [LICENSE](LICENSE) file for the full license text.

## HELP:
**if you counter issues please let me know, if you dont like the ui, then tell me, discord: *adilkab* and btw everything i make is free and open source, so support me by JUST joining my discord server: **https://discord.gg/TZCHGu9e3H***
