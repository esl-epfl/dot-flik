#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <numeric>
#include <getopt.h>  // For command-line argument parsing

#include "motionLib/motionLib.hpp"
using namespace motionLib;
#include "utils/utils.hpp"
using namespace utils;


int main(int argc, char** argv) {
    // Parse command-line arguments
    Arguments args{
        "../../media/londra1.mp4",
        "./filtered_motion.mp4",
        3,
        0.0,
        false
    };
    parseArguments(argc, argv, args);
    
    // Set default parameters
    cv::Size frameSize(640, 640);  // Full HD resolution
    // cv::Size frameSize(1080, 1080);  // RPi original resolution
    cv::Size blurKernel(5, 5);
    
    // Open input video
    cv::VideoCapture cap(args.videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open video file " << args.videoPath << std::endl;
        return -1;
    }
    
    // Get video properties
    double fps = cap.get(cv::CAP_PROP_FPS);
    int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    
    // Determine how many frames to read
    int maxFrames = totalFrames;
    if (args.hasSeconds) {
        maxFrames = std::min(static_cast<int>(args.seconds * fps), maxFrames);
    }
    
    std::cout << "FPS: " << fps << ", Total frames: " << totalFrames 
              << ", Processing: " << maxFrames << " frames" << std::endl;
    
    // Output video writer
    cv::VideoWriter out(args.outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), 
                        fps, frameSize, true);
    
    if (!out.isOpened()) {
        std::cerr << "Could not open the output video for write: " << args.outputPath << std::endl;
        return -1;
    }
    
    // Read and preprocess frames
    std::vector<cv::Mat> frames;
    frames.reserve(maxFrames); // Memory preallocation
    
    std::cout << "Reading and preprocessing frames..." << std::endl;
    ProgressBar readProgress(maxFrames);
    
    for (int i = 0; i < maxFrames; i++) {
        cv::Mat frame;
        if (!cap.read(frame)) {
            break;
        }
        frames.push_back(preprocessFrame(frame, frameSize, blurKernel));
        readProgress.update(i + 1);
    }
    readProgress.done();
    cap.release();
    
    // Process and write output frames
    std::cout << "Processing..." << std::endl;
    std::vector<double> perFrameTimes;
    perFrameTimes.reserve(frames.size() - 2);
    
    ProgressBar processProgress(frames.size() - 2, "Amplifying motion");
    
    for (size_t k = 1; k < frames.size() - 1; k++) {
        auto tStart = std::chrono::high_resolution_clock::now();
        
        const cv::Mat& prev = frames[k - 1];
        const cv::Mat& curr = frames[k];
        const cv::Mat& next = frames[k + 1];
        
        cv::Mat amplified = amplifyMotion(prev, curr, next, args.gamma);
        cv::Mat heatmap;
        cv::applyColorMap(amplified, heatmap, cv::COLORMAP_HOT);
        
        out.write(heatmap);
        
        auto tEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> frameTime = tEnd - tStart;
        perFrameTimes.push_back(frameTime.count());
        
        processProgress.update(k);
    }
    processProgress.done();
    
    out.release();
    
    // Calculate and display statistics
    double avgTime = std::accumulate(perFrameTimes.begin(), perFrameTimes.end(), 0.0) / perFrameTimes.size();
    double fpsEff = avgTime > 0 ? 1.0 / avgTime : 0.0;
    
    std::cout << "\n✅ Done!" << std::endl;
    std::cout << "⏱️  Avg frame time: " << avgTime * 1000.0 << " ms" << std::endl;
    std::cout << "⚡ Effective processing FPS: " << fpsEff << std::endl;
    std::cout << "💾 Output saved to: " << args.outputPath << std::endl;
    
    return 0;
}
