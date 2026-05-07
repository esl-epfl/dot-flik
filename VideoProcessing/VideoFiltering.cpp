#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>

#include "motionLib/motionLib.hpp"
using namespace motionLib;
#include "utils/utils.hpp"
using namespace utils;


int main(int argc, char** argv) {
    Arguments args{
        "../../media/londra1.mp4", // Relative to the current working directory!
        "./filtered_motion.mp4",
        3,
        0.0,
        false
    };
    parseArguments(argc, argv, args);
    
    // Set default parameters
    cv::Size blurKernel(5, 5);
    
    // Open input video
    cv::VideoCapture cap(args.videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open video file " << args.videoPath << std::endl;
        return -1;
    }
    
    // Get video properties...
    double fps = cap.get(cv::CAP_PROP_FPS);
    int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    double duration = totalFrames / fps;
    cv::Size frameSize = cv::Size(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)), 
                                static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    // ...print them out
    std::cout << "FPS: " << fps << " - Total frames: " << totalFrames 
        << " - Duration: " << duration << " seconds" << 
        " - Frame size: " << frameSize.width << "x" << frameSize.height << std::endl;
    
    // Determine how many frames to read
    int maxFrames = totalFrames;
    if (args.hasSeconds) {
        maxFrames = std::min(static_cast<int>(args.seconds * fps), maxFrames);
    }
    

    // ---------------------------------
    // Read and preprocess frames
    // ---------------------------------
    std::vector<cv::Mat> frames, originalFrames;
    frames.reserve(maxFrames);
    originalFrames.reserve(maxFrames);
    
    std::cout << "Reading and preprocessing frames..." << std::endl;
    ProgressBar readProgress(maxFrames);
    
    for (int i = 0; i < maxFrames; i++) {
        cv::Mat frame;
        if (!cap.read(frame)) {
            break;
        }
        originalFrames.push_back(frame);
        frames.push_back(preprocessFrame(frame, cv::Size(320,320), blurKernel));
        readProgress.update(i + 1);
    }
    readProgress.done();
    cap.release();
    // Check preprocess frames properties
    std::cout << "Preprocess frame properties (from frames[0]):" << std::endl;
    std::cout << "Frame size: " << frames[0].cols << "x" << frames[0].rows << " - ";
    std::cout << "Frame channels: " << frames[0].channels() << " - ";
    std::cout << "Depth: " << frames[0].depth() << " - ";
    std::cout << "Frame type: " << frames[0].type() << std::endl;
    
    // Process and write output frames
    std::vector<cv::Mat> processedFrames;
    processedFrames.reserve(maxFrames-2);

    std::cout << "Running motion amplification..." << std::endl;
    ProgressBar processProgress(frames.size() - 2, "Amplifying motion");
    
    for (size_t k = 1; k < frames.size() - 1; k++) {
        const cv::Mat& prev = frames[k - 1];
        const cv::Mat& curr = frames[k];
        const cv::Mat& next = frames[k + 1];
        
        cv::Mat amplified = amplifyMotion(prev, curr, next, args.gamma);
        processedFrames.push_back(amplified);
        
        processProgress.update(k);
    }
    processProgress.done();


    // --------------------------------
    // Filter video frames based on motion detection
    // --------------------------------
    std::cout << "Filtering video frames..." << std::endl;
    std::vector<cv::Mat> filteredFrames = filterVideo(originalFrames, processedFrames, 20, 2000);
    std::cout << "Saving filtered video..." << std::endl;
    cv::VideoWriter writer(args.outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, frameSize);
    if (!writer.isOpened()) {
        std::cerr << "Could not open the output video for write: " << args.outputPath << std::endl;
        return -1;
    }
    for (const auto& frame : filteredFrames) {
        writer.write(frame);
    }
    writer.release();

    std::cout << "Done!" << std::endl;
    return 0;
}
