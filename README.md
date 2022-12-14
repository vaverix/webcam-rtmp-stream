# webcam-rtmp-stream

Webcam capture streaming via RTMP or saving into video file. Cross-platform.
Sound device support added. Aimed mainly for Linux servers with no gpu.

## Inspirations

[jkuri/ffmpeg-webcam-rtmp-stream](https://github.com/jkuri/ffmpeg-webcam-rtmp-stream)

[freeeyes/ffmpeg4_camera_to_rtmp](https://github.com/freeeyes/ffmpeg4_camera_to_rtmp)

### Build (dynamic)

#### Prerequisites (Ubuntu)

```sh
sudo apt-get update
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev -y
sudo apt-get install ffmpeg -y
sudo apt-get install build-essential clang -y
```

#### Prerequisites (MacOS)

```sh
brew install ffmpeg
```

#### Prerequisites (Windows, not tested) [vcpkg](https://vcpkg.io/en/getting-started.html)

```sh
vcpkg install ffmpeg ffmpeg:x64-windows
```

### Installation

After you installed everything for your host OS, run:

```sh
make
```

Build artifacts will be stored inside `build/` directory.

### Usage

#### Parameters

```sh
./build/stream [video_device] [audio_device] [output_path] [output_format] [width] [height] [fps]
```

#### Linux example

```sh
./build/stream /dev/video0 default rtmp://localhost/live/stream flv 1920 1080 30
```

```sh
./build/stream /dev/video0 default rtmp://live.twitch.tv/app/streamkey flv 1920 1080 30
```

```sh
./build/stream /dev/video0 default rtmp://a.rtmp.youtube.com/live2/streamkey flv 1920 1080 30
```

### License

MIT
