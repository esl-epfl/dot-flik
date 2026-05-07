# Camera Streamer folder

This folder contains the code for the camera stream acquisition part of the pipeline, in C++.

It includes:
- Camera initialization and configuration (V4L2 backend)
- Video format negotiation (MJPG, YUYV, etc.)
- Frame capture and timing profiling
- Wi-Fi stream

## Components

- `cameraWifiStream.cpp`: Test GStreamer pipeline for Wi-Fi streaming. It captures video from the camera and streams it over the network using GStreamer.
- `CameraStreamer.cpp`: Test camera acquisition speed with different video formats and resolutions. Achieved mean capture speed is printed to the console for every combination of format and resolution. 
- `testFormat.cpp`: Small test application to check if requested video format and resolution are correctly provided by the camera.

## Camera Support

The streamer is designed to work with the **Raspberry Pi Camera Module V2**. Other camera system requires little to no modifications, as long as they support the V4L2 interface.

Other Raspberry Pi cameras are expected to work without modifications, but they have not been tested.


## Tested Formats

> **Warning:** stride (bytes per line) must be 32 bytes aligned with raw BGR/RGB formats. V4L2 will still enforce this alignment, but it will clash with the expected pixel rows in openCV.
>
> `v4l2-ctl --device=/dev/video0 --get-fmt-video`can be used to check the current format and alignment on V4L2 side.

Out of all the supported formats, the following have been tested and are known to work correctly:
- **MJPG (Motion-JPEG)**: Hardware-compressed format (utilize hardware encoder)
- **YU12/YV12**: Planar YUV formats
- **RGB3/BGR3**: Raw RGB formats


## GStreamer pipelines
Some GStreamer pipeline to be used directly from the command line for testing purposes are porovided below.

### Sender (Camera to Network)
```bash
gst-launch-1.0 -e v4l2src device=/dev/video0 \
    ! 'video/x-raw, format=NV12, width=640, height=640, framerate=30/1' \
    ! v4l2h264enc extra-controls="controls,video_gop_size=1,video_b_frames=0" \
    ! 'video/x-h264,level=(string)3.1' \
    ! h264parse \
    ! rtph264pay config-interval=1 pt=96 \
    ! 'application/x-rtp, clock-rate=90000' \
    ! udpsink host=192.168.70.64 port=5000
```

### Receiver (Network to File)
```bash
# Single file output
gst-launch-1.0 -e udpsrc port=5000 \
    ! application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96 \
    ! rtph264depay \
    ! h264parse \
    ! mp4mux \
    ! filesink location=received_video.mp4
```
### Receiver with Video Split
```bash
# Split videos into segments (10 second chunks)
gst-launch-1.0 -e udpsrc port=5000 \
    ! application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96 \
    ! rtph264depay \
    ! h264parse \
    ! splitmuxsink location=received_video_%05d.mp4 \
                   max-size-time=10000000000 \
                   muxer=mp4mux \
                   send-keyframe-requests=false
```

### Testing Pipeline
```bash
# Test with video test source (no camera required)
gst-launch-1.0 videotestsrc \
    ! 'video/x-raw, format=NV12, width=640, height=640, framerate=30/1' \
    ! v4l2h264enc extra-controls="controls,video_gop_size=1,video_b_frames=0" \
    ! 'video/x-h264,level=(string)3.1' \
    ! h264parse \
    ! rtph264pay config-interval=1 pt=96 \
    ! 'application/x-rtp, clock-rate=90000' \
    ! udpsink host=192.168.70.10 port=5000
```


---

-> Back to root [README](../README.md)