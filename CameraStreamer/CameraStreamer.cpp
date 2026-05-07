#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <string>

int totalFrames = 1000; // Total frames to capture

int main() {
    std::cout << "Testing camera formats and resolutions (totalFrames = "
                << totalFrames << ")" << std::endl;
    // /dev/video0 with V4L2 backend
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    
    // Check if camera opened successfully
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera" << std::endl;
        return -1;
    }
    
    // Set camera properties for Raspberry Pi Camera v2
    // Crucial to specify the format, not all resolutions are supported by all formats
    // MJPG (Motion-JPEG)
    const int MJPG = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    // JPEG (typically same as MJPG for video)
    const int JPEG = cv::VideoWriter::fourcc('J', 'P', 'E', 'G');
    // YUYV (Raw uncompressed format)
    const int YUYV = cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V');
    // YU12 (Planar YUV format)
    const int YU12 = cv::VideoWriter::fourcc('Y', 'U', '1', '2');
    // YV12 (Planar YUV format)
    const int YV12 = cv::VideoWriter::fourcc('Y', 'V', '1', '2');
    // RGB3 (Raw RGB format)
    const int RGB3 = cv::VideoWriter::fourcc('R', 'G', 'B', '3');
    // BGR3 (Raw BGR format)
    const int BGR3 = cv::VideoWriter::fourcc('B', 'G', 'R', '3');

    std::vector<int> formats = { MJPG, JPEG, YUYV, YU12, YV12, RGB3, BGR3 };
    std::vector<cv::Size> resolutions = {
        cv::Size(640, 480),   // 480p (VGA)
        cv::Size(1280, 720),  // 720p (HD)
        cv::Size(1920, 1080), // 1080p (Full HD)
        cv::Size(1080, 1080)  // Custom square resolution
    };


    // Try to reach fps as high as possible
    cap.set(cv::CAP_PROP_FPS, -1);
    std::cout << "Actual FPS: " << cap.get(cv::CAP_PROP_FPS) << std::endl;

    for (const int& format : formats) {
        for (const cv::Size& resolution : resolutions) {
            // Set the camera properties
            cap.set(cv::CAP_PROP_FOURCC, format);
            cap.set(cv::CAP_PROP_FRAME_WIDTH, resolution.width);
            cap.set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);

            // Print current camera properties
            char fourccString[5] = {};
            memcpy(fourccString, &format, 4);
            std::cout << "Testing format: " << fourccString
                      << " at resolution: " << resolution.width << "x" << resolution.height << std::endl;

            // Check if the format was set correctly
            int fourccGet = cap.get(cv::CAP_PROP_FOURCC);
            int widthGet = cap.get(cv::CAP_PROP_FRAME_WIDTH);
            int heightGet = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
            if (fourccGet != format || widthGet != resolution.width || heightGet != resolution.height) {
                std::cerr << "Error: Could not set format or resolution correctly." << std::endl;
                return 1;
            }

            int captureTimeTotal = 0;
            for (int i = 0; i < totalFrames; i++) {
                cv::Mat frame;
                auto frameStart = std::chrono::high_resolution_clock::now();
                cap >> frame;
                auto captureEnd = std::chrono::high_resolution_clock::now();
                if (frame.empty()) {
                    std::cerr << "Error: Could not capture frame " << std::endl;
                    return 1;
                }
                auto captureTime = std::chrono::duration_cast<std::chrono::microseconds>(captureEnd - frameStart);
                captureTimeTotal += captureTime.count() / 1000.0;
            }
            std::cout << "Mean Capture Time: " << captureTimeTotal/totalFrames << " ms/frame";
            std::cout << " -> frame rate: " << 1000 / (captureTimeTotal/totalFrames) << " fps" << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Release resources
    cap.release();
    
    return 0;
}
