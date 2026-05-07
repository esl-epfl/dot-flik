# Full Pipeline folder
## This folder contains the complete end-to-end pipeline implementations, in C++.

The full pipeline integrates all components of the motion detection and streaming system, including:
- Camera capture (V4L2/GStreamer backends)
- Motion preprocessing and amplification
- Motion detection with configurable thresholds
- H.264 video encoding and UDP/RTP streaming
- Performance profiling and statistics collection
- Configurable frame dropping for bandwidth optimization

## Components

- `fullPipeline.cpp`: Basic motion-informed streaming pipeline. Captures frames from camera, processes them for motion detection, and streams frames with detected motion over WiFi using GStreamer H.264/RTP pipeline.

- `fullPipeline_profile.cpp`: Profiling version of the pipeline with performance tracking. Includes timing statistics for camera capture, motion processing, and streaming operations. Supports configurable frame drop percentage and generates CSV performance reports.

- `fullPipeline_threaded.cpp`: Multi-threaded version using producer-consumer pattern with thread-safe queues. Separates camera capture, motion processing, and streaming into independent threads.

- `fullPipeline_threaded_profile.cpp`: Threaded pipeline with profiling capabilities. Combines multi-threading architecture with performance monitoring across all pipeline stages.

- `fullPipeline_externalVideos.cpp`: Pipeline variant that reads from pre-recorded video files instead of live camera input. Useful for testing and development without requiring camera hardware.


## Configuration Parameters

### Motion Detection
- `GAMMA = 3`: Motion amplification factor
- `KSIZE = 20`: Motion detection kernel size
- `THRESHOLD = 2000`: Motion detection sensitivity threshold

### Camera Settings
- **Resolution**: 640x640 (square format for AI pipeline)
- **Format**: YU12 (Planar YUV) or YUY2 depending on backend
- **Frame Rate**: Configurable via command line (default: 30 FPS)

### Network Settings
- **Default Host**: `192.168.18.168`
- **Default Port**: `5000`
- **Protocol**: UDP/RTP with H.264 payload

## Command Line Options (Profile Versions)


```bash
./fullPipeline_profile [OPTIONS]

Options:
  --duration, -d <seconds>    Set test duration in seconds (default: 30)
  --fps, -f <fps>            Set frames per second (default: 30)
  --host, -H <host>          Set the host address (default: 192.168.18.168)
  --port, -P <port>          Set the port number (default: 5000)
  --drop, -D <percentage>    Set percentage of frames to drop (0-100, default: 0)
  --single-thread, -s        Use single thread for OpenCV operations
  --help, -h                 Show help message
```

> **Warning:** The `--single-thread` option may not be properly respected by the system. OpenCV and underlying libraries may still utilize multiple threads despite this flag being set. This appears to be a limitation of how `cv::setNumThreads()` interacts with the underlying threading model of OpenCV and its dependencies.

## GStreamer Pipeline

### Camera Capture (when using GStreamer backend)
```bash
v4l2src device=/dev/video0 
! video/x-raw,format=YUY2,width=640,height=640,framerate=30/1 
! videoconvert 
! video/x-raw,format=BGR 
! appsink
```

### Video Streaming
```bash
appsrc 
! videoconvert 
! video/x-raw,format=NV12,width=640,height=640,framerate=30/1 
! v4l2h264enc extra-controls=controls,video_gop_size=1,video_b_frames=0 
! video/x-h264,level=(string)3.1 
! h264parse 
! rtph264pay config-interval=1 pt=96 
! application/x-rtp,clock-rate=90000 
! udpsink host=<HOST> port=<PORT>
```

---

-> Back to root [README](../README.md)
