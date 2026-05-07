#include <opencv2/opencv.hpp>
#include <iostream>

#include "motionLib/motionLib.hpp"
#include "utils/utils.hpp"

int main() {

    // /dev/video0 with V4L2 backend
    cv::VideoCapture cap(0, cv::CAP_V4L2);

    // Check if camera opened successfully
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera" << std::endl;
        return 1;
    }

    // MJPG (Motion-JPEG) format
    static int videoFormat = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    cap.set(cv::CAP_PROP_FOURCC, videoFormat);

    // 1080 x 1080 at 30fps
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1080);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
    cap.set(cv::CAP_PROP_FPS, 30);


    // Read back and print camera properties
    std::cout << "Camera format verification:" << std::endl;
    std::cout << "Requested FOURCC: MJPG" << std::endl;
    std::cout << "Actual FOURCC: " << cap.get(cv::CAP_PROP_FOURCC) << std::endl;
    std::cout << "Width: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << std::endl;
    std::cout << "Height: " << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
    std::cout << "FPS: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
    // Capture a test frame to see its properties
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "Error: Could not capture test frame" << std::endl;
        return -1;
    }
    std::cout << "\nFrame properties:" << std::endl;
    std::cout << "Frame size: " << frame.cols << "x" << frame.rows << std::endl;
    std::cout << "Frame channels: " << frame.channels() << std::endl;
    std::cout << "Depth: " << frame.depth() << std::endl;
    std::cout << "Frame type: " << frame.type() << std::endl;


    return 0;
}