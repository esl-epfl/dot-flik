#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <string>

#include "motionLib/motionLib.hpp"

// How many seconds to reconrd and send
const int RECORD_SECONDS = 10;
const int FPS = 30;
const int TOTAL_FRAMES = RECORD_SECONDS * FPS;
const int GAMMA = 3;
// Motion detection hyperparameters
const int KSIZE = 20;
const int THRESHOLD = 2000;
// Where to send the stream
const std::string HOST = "192.168.18.168";

int main() {
    std::cout << "WiFi frame stream with motion informed frame drop" << std::endl;

    // --------------
    // CAMERA SETUP
    // --------------

    /*
    GStreamer pipeline for camera capture using V4L2:
    1. v4l2src device=/dev/video0 - V4L2 source element capturing from camera device
    2. video/x-raw,format=YU12,width=640,height=640,framerate=30/1 - Caps filter specifying:
       - NV12 format (Planar YUV format, same as before)
       - 640x640 resolution (square format for AI pipeline)
       - 30 fps framerate (or user-specified fps)
    3. videoconvert - Converts video format as needed for OpenCV
    4. appsink - Sink element that provides frames to application code
    */
    std::string camera_pipeline = 
        "v4l2src device=/dev/video0 "
        "! video/x-raw,format=NV12,width=640,height=640,framerate=30/1 "
        "! videoconvert "
        "! video/x-raw,format=BGR,width=640,height=640"
        "! appsink";
    
    cv::VideoCapture cap(camera_pipeline, cv::CAP_GSTREAMER);
    // Check if camera opened successfully
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera with GStreamer pipeline" << std::endl;
        std::cerr << "Pipeline: " << camera_pipeline << std::endl;
        return -1;
    }

    // Custom square resolution (already set in pipeline, but store for reference)
    const cv::Size resolution = cv::Size(640, 640); // Square format for AI pipeline


    // ----------------
    // WIFI STREAM SETUP
    // ----------------
    /*
    GStreamer pipeline for H.264 video streaming over UDP/RTP:
    1. appsrc - Source element that accepts video frames from application code
    2. videoconvert - Converts video format/colorspace as needed for downstream elements
    3. video/x-raw,format=NV12,width=640,height=640,framerate=30/1 - Caps filter specifying:
       - NV12 format (YUV 4:2:0 with interleaved UV plane). Requires minimal conversion in openCV,
       still faster than RGB to produce.
       - 640x640 resolution
       - 30 fps framerate
    4. v4l2h264enc - Hardware H.264 encoder using Video4Linux2 interface with:
       - GOP size of 1 (every frame is a keyframe, increases bandwidth but reduces latency)
       - 0 B-frames (no bidirectional prediction frames, reduces encoding latency)
    5. video/x-h264,level=(string)3.1 - H.264 level 3.1 constraint (supports up to 1280x720@30fps)
    6. h264parse - Parses H.264 stream and adds metadata needed for RTP packetization
    7. rtph264pay - Packetizes H.264 stream into RTP packets with:
       - config-interval=1 (send SPS/PPS with every second for robustness)
       - pt=96 (RTP payload type 96, dynamic payload type for H.264)
    8. application/x-rtp,clock-rate=90000 - RTP caps with 90kHz clock rate (standard for video)
    9. udpsink - Sends RTP packets via UDP to specified host and port 5000
    */
    std::string pipeline =
        "appsrc ! videoconvert "
        "! video/x-raw,format=NV12,width=640,height=640,framerate=30/1 "
        "! v4l2h264enc extra-controls=controls,video_gop_size=1,video_b_frames=0 "
        "! video/x-h264,level=(string)3.1 "
        "! h264parse "
        "! rtph264pay config-interval=1 pt=96 "
        "! application/x-rtp,clock-rate=90000 "
        "! udpsink host=" + HOST + " port=5000";
    cv::VideoWriter out(pipeline, cv::CAP_GSTREAMER, 0, FPS, resolution, true);
    if (!out.isOpened()) {
        std::cerr << "Failed to open video writer." << std::endl;
        return -1;
    }


    // ----------------------------
    // MOTION-INFORMED FRAME DROP LOOP
    // ----------------------------
    // Rolling buffers
    std::array<cv::Mat, 3> frames;
    std::array<cv::Mat, 3> preprocFrames;
    int rollingIndex = 0;
    // Allocate variables
    cv::Mat amplified;

    // Fill rolling buffer
    for (int initialIndex = 0; initialIndex < 3; initialIndex++) {
        cap >> frames[initialIndex];
        if (frames[initialIndex].empty()) {
            std::cerr << "Error: Could not capture frame" << std::endl;
            break;
        }

        preprocFrames[initialIndex] = motionLib::preprocessFrame(
            frames[initialIndex],
            cv::Size(320, 320), 
            cv::Size(5, 5)
        );
    }

    // Main loop body
    // for (int i = 0; i < TOTAL_FRAMES; i++) {
    for (;;) {
        // Get the frame from camera
        cap >> frames[rollingIndex];
        if (frames[rollingIndex].empty()) {
            std::cerr << "Error: Could not capture frame" << std::endl;
            break;
        }

        // Resize it to 320x320 and preprocess with 5x5 blur kernel
        preprocFrames[rollingIndex] = motionLib::preprocessFrame(
            frames[rollingIndex],
            cv::Size(320, 320), 
            cv::Size(5, 5)
        );

        // Process the frames...
        // (i+1)%3 - (i+2)%3 - i
        amplified = motionLib::amplifyMotion(
            preprocFrames[(rollingIndex+1)%3],  // t-2
            preprocFrames[(rollingIndex+2)%3],  // t-1
            preprocFrames[rollingIndex],        // t
            GAMMA
        );

        // Send original frame to Flick over WiFi IF enough motion is detected
        if (motionLib::motionDetection(amplified, KSIZE, THRESHOLD)) {
            out.write(frames[rollingIndex]);
        }

        // Update rolling index
        rollingIndex = (rollingIndex + 1) % 3;
    }

    std::cout << "Done!" << std::endl;
    cap.release();

    return 0;
}
