#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <string>

// How many seconds to reconrd and send
const int RECORD_SECONDS = 10;
const int FPS = 30;
const int TOTAL_FRAMES = RECORD_SECONDS * FPS;

int main() {
    std::cout << "Testing WiFi frame stream" << std::endl;

    // --------------
    // CAMERA SETUP
    // --------------

    // /dev/video0 with V4L2 backend
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    // Check if camera opened successfully
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera" << std::endl;
        return -1;
    }

    // YU12 (Planar YUV format)
    const int YU12 = cv::VideoWriter::fourcc('Y', 'U', '1', '2');
    // Custom square resolution
    const cv::Size resolution = cv::Size(640, 640); // 720p (HD)
    // Set the camera properties
    cap.set(cv::CAP_PROP_FPS, FPS);
    cap.set(cv::CAP_PROP_FOURCC, YU12);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, resolution.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);


    // ----------------
    // WIFI STREAM SETUP
    // ----------------

    std::string pipeline =
        "appsrc ! videoconvert "
        "! video/x-raw,format=NV12,width=640,height=640,framerate=30/1 "
        "! v4l2h264enc extra-controls=controls,video_gop_size=1,video_b_frames=0 "
        "! video/x-h264,level=(string)3.1 "
        "! h264parse "
        "! rtph264pay config-interval=1 pt=96 "
        "! application/x-rtp,clock-rate=90000 "
        "! udpsink host=192.168.18.168 port=5000";
    cv::VideoWriter out(pipeline, cv::CAP_GSTREAMER, 0, FPS, resolution, true);
    if (!out.isOpened()) {
        std::cerr << "Failed to open video writer." << std::endl;
        return -1;
    }


    // --------------
    // STREAM LOOP
    // --------------
    cv::Mat frame;
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "Error: Could not capture test frame" << std::endl;
            return -1;
        }

        out.write(frame);
    }

    std::cout << "Done!" << std::endl;
    cap.release();

    return 0;
}
