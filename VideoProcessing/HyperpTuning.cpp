#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "motionLib/motionLib.hpp"
using namespace motionLib;
#include "utils/utils.hpp"
using namespace utils;


int main(int argc, char** argv) {
    // Parse command-line arguments
    Arguments args{
        "../../media/londra1.mp4",
        "./hyperpTuningResults.csv",
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
    std::vector<cv::Mat> frames;
    frames.reserve(maxFrames); // Memory preallocation
    
    std::cout << "Reading and preprocessing frames..." << std::endl;
    ProgressBar readProgress(maxFrames);
    
    for (int i = 0; i < maxFrames; i++) {
        cv::Mat frame;
        if (!cap.read(frame)) {
            break;
        }
        frames.push_back(preprocessFrame(frame, cv::Size(320,320), blurKernel)); // Downsize to 320x320!
        readProgress.update(i + 1);
    }
    readProgress.done();
    cap.release();

    // Check preprocess frames properties
    std::cout << "Preprocess frame properties:" << std::endl;
    std::cout << "Frame size: " << frames[0].cols << "x" << frames[0].rows << " - ";
    std::cout << "Frame channels: " << frames[0].channels() << " - ";
    std::cout << "Depth: " << frames[0].depth() << " - ";
    std::cout << "Frame type: " << frames[0].type() << std::endl;

    
    // Process and write output frames
    std::vector<cv::Mat> outputFrames;
    outputFrames.reserve(maxFrames-2);

    std::cout << "Running motion amplification..." << std::endl;
    ProgressBar processProgress(frames.size() - 2, "Amplifying motion");
    
    for (size_t k = 1; k < frames.size() - 1; k++) {
        const cv::Mat& prev = frames[k - 1];
        const cv::Mat& curr = frames[k];
        const cv::Mat& next = frames[k + 1];
        
        cv::Mat amplified = amplifyMotion(prev, curr, next, args.gamma);
        outputFrames.push_back(amplified);
        
        processProgress.update(k);
    }
    processProgress.done();


    // --------------------------------
    // Hyperparameter tuning on amplified frames
    // --------------------------------
    // Parameters for hyperparameter tuning
    // std::array<int, 4> kernelSizes = {3, 5, 27, 45};
    std::array<int, 4> kernelSizes = {4, 8, 20, 40}; // for 320x320
    int maxThreshold = 200000; // 45 * 45 * 255 = 516375
    int minThreshold = 0;
    int stepThreshold = 100;

    std::cout << "Running hyperparameter tuning..." << std::endl;
    int totalOptions = 0, currentOptions = 0;
    totalOptions = kernelSizes.size() * (maxThreshold - minThreshold) / stepThreshold;
    ProgressBar tuningProgress(totalOptions, "Tuning");

    // Structure to hold results
    struct HyperpTuningResults {
        int kernelSize;
        int threshold;
        double score;
    };
    std::vector<HyperpTuningResults> results;
    results.reserve(totalOptions);
    
    int detectedMotionCount = 0;
    // For different kernel sizes
    for (const int& kernelSize : kernelSizes) {
        // For different thresholds
        for (int threshold = minThreshold; threshold <= maxThreshold; threshold += stepThreshold) {
            detectedMotionCount = 0;
            for (const auto& frame : outputFrames) {
                if (motionDetection(frame, kernelSize, threshold)) {
                    detectedMotionCount++;
                }
            }
            // Store results
            double score = static_cast<double>(detectedMotionCount) / outputFrames.size();
            results.push_back({kernelSize, threshold, score});

            currentOptions++;
            tuningProgress.update(currentOptions);
        }
    }
    tuningProgress.done();


    // Dump the results in a csv-formattes file
    std::ofstream resultsFile(args.outputPath);
    if (!resultsFile.is_open()) {
        std::cerr << "Could not open results file for writing." << std::endl;
        return -1;
    }
    resultsFile << "KernelSize,Threshold,Score\n";
    for (const auto& result : results) {
        resultsFile << result.kernelSize << "," 
                    << result.threshold << "," 
                    << result.score << "\n";
    }
    resultsFile.close();
    std::cout << "Results saved to hyperp_tuning_results.csv" << std::endl;
    

    std::cout << "Done!" << std::endl;
    return 0;
}
