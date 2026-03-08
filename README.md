#  WebRTC Streamer

A live video streaming solution built in **C++** using **GStreamer** and **WebRTC**. Stream from any V4L2 webcam or IP camera directly to a browser with sub-second latency — no plugins, no Flash, no proprietary software.

Supports multiple cameras, HLS recording to disk, and browser-based playback with a date/time picker.

---

##  Features

| Feature | Details |
|---|---|
| **Live WebRTC streaming** | Sub-second latency directly in the browser, no plugins required |
| **Multi-camera** | Configure any number of V4L2 devices in a single config file |
| **HLS recording** | Every session saved to disk as seekable `.ts` segments |
| **Browser playback** | Date + time picker to seek any past recording session |
| **Timestamp overlay** | Live date/time burned into every video frame |
| **Auto-reconnect** | Streamer automatically reconnects to the server after a network drop |

---


**Signalling flow:**
1. C++ streamer connects and registers each camera over WebSocket
2. Browser connects and receives the camera list
3. Browser clicks Watch → server forwards signal to the C++ streamer
4. C++ creates SDP offer → server relays it to the browser
5. Browser answers → ICE candidates exchanged → live video flows
6. Simultaneously, GStreamer writes HLS segments to `build/recordings/`

---

##  Requirements

- Ubuntu 22.04 or 24.04 (or any Debian-based Linux)
- GStreamer 1.20+
- Node.js 18+
- CMake 3.16+
- A V4L2-compatible webcam or IP camera at `/dev/videoN`

---

##  Setup & Build

### Step 1 — Install system dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-nice \
    libnice-dev \
    libsoup-3.0-dev \
    libjson-glib-dev
```

### Step 2 — Verify GStreamer plugins

All three plugins must be present before building. Run each command and confirm it prints plugin details rather than "No such element":

```bash
gst-inspect-1.0 webrtcbin
gst-inspect-1.0 hlssink2
gst-inspect-1.0 x264enc
```

If `webrtcbin` is missing:
```bash
sudo apt install -y gstreamer1.0-plugins-bad gstreamer1.0-nice libnice-dev
```

If `x264enc` is missing:
```bash
sudo apt install -y gstreamer1.0-plugins-ugly
```

If `hlssink2` is missing:
```bash
sudo apt install -y gstreamer1.0-plugins-bad
```

Verify your camera is visible to the system:
```bash
v4l2-ctl --list-devices
# Should list /dev/video0 or similar
```

### Step 3 — Install Node.js

```bash
# Option A — via apt (check version is 18+)
sudo apt install -y nodejs npm
node --version

```

### Step 4 — Install Node.js server dependencies

```bash
cd Server
npm install
cd ..
```


### Step 5 — Build the C++ streamer

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
cd ..
```


##  Configuration

### cameras.conf

List one camera per line. Format: `/dev/videoN  Display Name`

```bash
# Lines starting with # are ignored
# Format: device_path    Display Name
/dev/video0  Front Door
/dev/video3  Balcony
```




##  Running

Open **three separate terminals**.

### Terminal 1 — Signalling server

```bash
cd Server
node server.js
```


### Terminal 2 — C++ streamer

```bash
cd build
./webrtc-streamer --config ../cameras.conf --server http://127.0.0.1:57778 --user MyCam
```

**CLI flags:**

| Flag | Short | Default | Description |
|---|---|---|---|
| `--server` | `-s` | `http://127.0.0.1:57778` | Signalling server URL |
| `--user` | `-u` | `streamer` | Name shown in the server log |
| `--config` | `-c` | `cameras.conf` | Path to camera config file |



---

### Terminal 3 — Browser client

```bash
# Option A — open the file directly
xdg-open Web-Client/client.html

```


---

##  Recording & Playback

When a viewer connects, GStreamer simultaneously sends live WebRTC video to the browser and writes HLS segments to disk. Each session gets its own timestamped folder:

```
build/recordings/
└── video0_20260309_142510/
    ├── playlist.m3u8        ← hls.js loads this
    ├── segment_00000.ts     ← 6-second chunks
    ├── segment_00001.ts
    └── ...
```

**Using the playback UI:**
1. Click **Playback /dev/video0** in the browser
2. The panel shows how many sessions are available
3. Select a date and time using the pickers
4. Click **Play** — the session closest to that time loads automatically
5. Use the video scrubbar to seek within the session
6. Click **Close** to stop


---

##  Stopping

Press `Ctrl+C` in each terminal. The streamer sends an EOS event before exiting so the current HLS segment is finalised and the last recording is fully playable.

---


##  Project Structure

```
project/
├── src/
│   ├── main.cpp              # CLI entry point, SIGINT/SIGTERM handling
│   ├── WebRTCApp.cpp         # WebSocket connection, reconnect, message routing
│   ├── WebRTCStream.cpp      # Per-camera GStreamer pipeline + WebRTC logic
│   └── Logger.cpp            # Thread-safe logger (stdout + log.txt)
├── include/
│   ├── Config.h              # All compile-time constants
│   ├── CameraConfig.h        # cameras.conf parser
│   ├── WebRTCApp.h
│   ├── WebRTCStream.h
│   └── Logger.h
├── Server/
│   └── server.js             # Node.js signalling server + HTTP for HLS files
├── Web-Client/
│   ├── client.html           # Browser UI
│   └── script.js             # WebRTC + playback logic
├── cameras.conf              # Camera device list — edit this
├── CMakeLists.txt
└── build/
    ├── webrtc-streamer       # Compiled binary (created by make)
    └── recordings/           # HLS sessions written here at runtime
```

