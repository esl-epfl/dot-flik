#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <random>

#include "motionLib/motionLib.hpp"
using namespace motionLib;
#include "utils/utils.hpp"
using namespace utils;

struct ProfilingStats {
    double preprocessingTime;
    double amplificationTime;
    double decisionTime;
    int iterations;
};

void printStats(const ProfilingStats& stats) {
    std::cout << "\n=== PROFILING RESULTS ===" << std::endl;
    std::cout << "Iterations: " << stats.iterations << std::endl;
    std::cout << "Average preprocessing time: " << (stats.preprocessingTime / stats.iterations) * 1000 << " ms/iteration" << std::endl;
    std::cout << "Average amplification time: " << (stats.amplificationTime / stats.iterations) * 1000 << " ms/iteration" << std::endl;
    std::cout << "Average decision time: " << (stats.decisionTime / stats.iterations) * 1000 << " ms/iteration" << std::endl;
    std::cout << "Total preprocessing time: " << stats.preprocessingTime << " seconds" << std::endl;
    std::cout << "Total amplification time: " << stats.amplificationTime << " seconds" << std::endl;
    std::cout << "Total decision time: " << stats.decisionTime << " seconds" << std::endl;
    std::cout << "=========================" << std::endl;
}

int main(int argc, char** argv) {
    // Parse command-line arguments
    Arguments args{
        "../../media/londra1.mp4",
        "./dummy_output.mp4",  // Won't actually write
        3,
        0.0,
        false
    };
    parseArguments(argc, argv, args);
    
    // Profiling parameters
    const int MAX_ITERATIONS = 1000;  // How many times to repeat processing
    
    ProfilingStats stats{};
    stats.iterations = MAX_ITERATIONS;
    cv::Size blurKernel(5, 5);
    
    // Load video and get properties
    cv::VideoCapture cap(args.videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open video file " << args.videoPath << std::endl;
        return -1;
    }
    
    int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    cv::Size frameSize(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)), 
                      static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    
    // Load 3 random frames
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, totalFrames - 1);
    
    std::vector<int> randomFramesIndex(3);
    for (int i = 0; i < 3; i++) {
        randomFramesIndex[i] = dis(gen);
    }
    std::sort(randomFramesIndex.begin(), randomFramesIndex.end());
    
    std::vector<cv::Mat> frames(3);
    int j = 0;
    for (int i = 0; i < totalFrames-1; i++) {
        cv::Mat frame;
        if (i == randomFramesIndex[j]) {
            if (!cap.read(frame)) {
                std::cerr << "Failed to read frame " << i << std::endl;
                return -1;
            }
            frames[j] = frame;
            j++;
            if (j >= 3) break;  // Only need 3 frames
        } else {
            cap.grab();  // Skip this frame
        }
    }
    cap.release();
    
    std::cout << "Starting motion profiling..." << std::endl;
    std::cout << "Max iterations: " << MAX_ITERATIONS << std::endl;
    std::cout << "Using frames " << randomFramesIndex[0] << ", " << randomFramesIndex[1] << ", " << randomFramesIndex[2] << std::endl;

    ProgressBar ProgressBar(MAX_ITERATIONS, "Processing iterations", 50);
    
    for (int iteration = 0; iteration < MAX_ITERATIONS; iteration++) {
        // Preprocess frames
        std::vector<cv::Mat> preprocFrames(3);
        auto preprocessStart = std::chrono::high_resolution_clock::now();
        preprocFrames[0] = preprocessFrame(frames[0], frameSize, blurKernel);
        preprocFrames[1] = preprocessFrame(frames[1], frameSize, blurKernel);
        preprocFrames[2] = preprocessFrame(frames[2], frameSize, blurKernel);
        auto preprocessEnd = std::chrono::high_resolution_clock::now();
        stats.preprocessingTime += std::chrono::duration<double>(preprocessEnd - preprocessStart).count();

        // Motion amplification
        auto amplifyStart = std::chrono::high_resolution_clock::now();
        cv::Mat amplified = amplifyMotion(preprocFrames[0], preprocFrames[1], preprocFrames[2], args.gamma);
        auto amplifyEnd = std::chrono::high_resolution_clock::now();
        stats.amplificationTime += std::chrono::duration<double>(amplifyEnd - amplifyStart).count();
        
        // Filtering
        auto filterStart = std::chrono::high_resolution_clock::now();
        motionDetection(amplified, 27, 18700);  // Using default kernel size and threshold
        auto filterEnd = std::chrono::high_resolution_clock::now();
        stats.decisionTime += std::chrono::duration<double>(filterEnd - filterStart).count();
        
        ProgressBar.update(iteration + 1);
        
    }
    ProgressBar.done();
    
    std::cout << std::endl;
    
    // Final results
    printStats(stats);
    
    // Additional system info
    std::cout << "\n=== SYSTEM INFO ===" << std::endl;
    std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << std::endl;
    
    return 0;
}