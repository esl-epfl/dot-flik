#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <iomanip>

#include "motionLib/motionLib.hpp"

namespace fs = std::filesystem;

// Motion detection hyperparameters
const int GAMMA = 3;
const int KSIZE = 20;
const int THRESHOLD = 2000;

// Visualization flag - set to true to output heatmap side-by-side with original frame
const bool ENABLE_HEATMAP_VISUALIZATION = true;

// Square output dimensions for fitting in single column of 2-column paper
const int SQUARE_OUTPUT_SIZE = 720;

// Folder paths - modify these as needed
const std::string INPUT_FOLDER = "/Users/mattia/MTh/newMeasurements/orlando-butterflies";
const std::string OUTPUT_FOLDER = "/Users/mattia/MTh/newMeasurements/orlando-butterflies_processed";
const std::string CSV_OUTPUT = "/Users/mattia/MTh/newMeasurements/processing_results.csv";

struct VideoStats {
    std::string filename;
    double original_duration_sec;
    double processed_duration_sec;
    int total_frames;
    int dropped_frames;
    double drop_percentage;
};

void writeCSVHeader(std::ofstream& csv) {
    csv << "# Hyperparameters\n";
    csv << "# GAMMA," << GAMMA << "\n";
    csv << "# KSIZE," << KSIZE << "\n";
    csv << "# THRESHOLD," << THRESHOLD << "\n";
    csv << "#\n";
    csv << "Filename,Original Duration (s),Processed Duration (s),Total Frames,Dropped Frames,Drop Percentage (%)\n";
}

VideoStats processVideo(const std::string& input_path, const std::string& output_path) {
    VideoStats stats;
    stats.filename = fs::path(input_path).filename().string();
    
    // Open input video
    cv::VideoCapture cap(input_path);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video: " << input_path << std::endl;
        stats.total_frames = 0;
        stats.dropped_frames = 0;
        stats.original_duration_sec = 0;
        stats.processed_duration_sec = 0;
        stats.drop_percentage = 0;
        return stats;
    }

    // Get video properties
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    stats.total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    stats.original_duration_sec = stats.total_frames / fps;
    
    std::cout << "Processing: " << stats.filename << std::endl;
    std::cout << "  Resolution: " << frame_width << "x" << frame_height << std::endl;
    std::cout << "  FPS: " << fps << std::endl;
    std::cout << "  Total frames: " << stats.total_frames << std::endl;

    // Setup output video writer
    // Use square format for paper-friendly output
    cv::VideoWriter out(output_path, 
                        cv::VideoWriter::fourcc('m','p','4','v'),
                        fps, 
                        cv::Size(SQUARE_OUTPUT_SIZE, SQUARE_OUTPUT_SIZE),
                        true);
    
    if (!out.isOpened()) {
        std::cerr << "Error: Could not open output video writer: " << output_path << std::endl;
        cap.release();
        stats.dropped_frames = stats.total_frames;
        stats.drop_percentage = 100.0;
        stats.processed_duration_sec = 0;
        return stats;
    }

    // Rolling buffers for motion detection
    std::array<cv::Mat, 3> frames;
    std::array<cv::Mat, 3> preprocFrames;
    int rollingIndex = 0;
    cv::Mat amplified;
    cv::Mat heatmap;
    cv::Mat combined;
    
    int frames_written = 0;
    
    // Fill rolling buffer with first 3 frames
    for (int initialIndex = 0; initialIndex < 3; initialIndex++) {
        cap >> frames[initialIndex];
        if (frames[initialIndex].empty()) {
            std::cerr << "Error: Not enough frames in video" << std::endl;
            break;
        }

        preprocFrames[initialIndex] = motionLib::preprocessFrame(
            frames[initialIndex],
            cv::Size(320, 320), 
            cv::Size(5, 5)
        );
    }

    // Main processing loop
    int frame_count = 3; // Already read 3 frames
    while (true) {
        // Read next frame
        cap >> frames[rollingIndex];
        if (frames[rollingIndex].empty()) {
            break;
        }
        
        frame_count++;

        // Preprocess frame for motion detection
        preprocFrames[rollingIndex] = motionLib::preprocessFrame(
            frames[rollingIndex],
            cv::Size(320, 320), 
            cv::Size(5, 5)
        );

        // Amplify motion between consecutive frames
        amplified = motionLib::amplifyMotion(
            preprocFrames[(rollingIndex+1)%3],  // t-2
            preprocFrames[(rollingIndex+2)%3],  // t-1
            preprocFrames[rollingIndex],        // t
            GAMMA
        );

        // In heatmap visualization mode, process all frames without dropping
        // In normal mode, write frame only if enough motion is detected
        bool shouldWrite = ENABLE_HEATMAP_VISUALIZATION || 
                          motionLib::motionDetection(amplified, KSIZE, THRESHOLD);
        
        if (shouldWrite) {
            if (ENABLE_HEATMAP_VISUALIZATION) {
                // Create heatmap from the motion amplified image at full resolution
                cv::Mat amplifiedFullRes = motionLib::amplifyMotion(
                    frames[(rollingIndex+1)%3],  // t-2 at full res
                    frames[(rollingIndex+2)%3],  // t-1 at full res
                    frames[rollingIndex],         // t at full res
                    GAMMA
                );
                
                // Convert to grayscale if needed
                cv::Mat amplifiedGray;
                if (amplifiedFullRes.channels() == 3) {
                    cv::cvtColor(amplifiedFullRes, amplifiedGray, cv::COLOR_BGR2GRAY);
                } else {
                    amplifiedGray = amplifiedFullRes;
                }
                
                // Normalize to enhance visibility (0-255 range)
                cv::normalize(amplifiedGray, amplifiedGray, 0, 255, cv::NORM_MINMAX);
                
                // Create custom black-to-red heatmap
                // Black (0,0,0) for low motion -> Red (0,0,255) for high motion
                heatmap = cv::Mat::zeros(amplifiedGray.size(), CV_8UC3);
                for (int y = 0; y < amplifiedGray.rows; y++) {
                    for (int x = 0; x < amplifiedGray.cols; x++) {
                        uchar intensity = amplifiedGray.at<uchar>(y, x);
                        // BGR format: set only the Red channel (index 2)
                        heatmap.at<cv::Vec3b>(y, x)[2] = intensity;
                    }
                }
                
                // Combine original frame and heatmap side by side
                cv::hconcat(frames[rollingIndex], heatmap, combined);
                
                // Resize to square format while maintaining aspect ratio
                cv::Mat squareOutput = cv::Mat::zeros(SQUARE_OUTPUT_SIZE, SQUARE_OUTPUT_SIZE, CV_8UC3);
                
                double scale = std::min(
                    static_cast<double>(SQUARE_OUTPUT_SIZE) / combined.cols,
                    static_cast<double>(SQUARE_OUTPUT_SIZE) / combined.rows
                );
                
                int scaled_width = static_cast<int>(combined.cols * scale);
                int scaled_height = static_cast<int>(combined.rows * scale);
                
                cv::Mat resized;
                cv::resize(combined, resized, cv::Size(scaled_width, scaled_height));
                
                // Center the resized image in the square canvas
                int x_offset = (SQUARE_OUTPUT_SIZE - scaled_width) / 2;
                int y_offset = (SQUARE_OUTPUT_SIZE - scaled_height) / 2;
                resized.copyTo(squareOutput(cv::Rect(x_offset, y_offset, scaled_width, scaled_height)));
                
                out.write(squareOutput);
            } else {
                // Resize single frame to square format while maintaining aspect ratio
                cv::Mat squareOutput = cv::Mat::zeros(SQUARE_OUTPUT_SIZE, SQUARE_OUTPUT_SIZE, CV_8UC3);
                
                double scale = std::min(
                    static_cast<double>(SQUARE_OUTPUT_SIZE) / frames[rollingIndex].cols,
                    static_cast<double>(SQUARE_OUTPUT_SIZE) / frames[rollingIndex].rows
                );
                
                int scaled_width = static_cast<int>(frames[rollingIndex].cols * scale);
                int scaled_height = static_cast<int>(frames[rollingIndex].rows * scale);
                
                cv::Mat resized;
                cv::resize(frames[rollingIndex], resized, cv::Size(scaled_width, scaled_height));
                
                // Center the resized image in the square canvas
                int x_offset = (SQUARE_OUTPUT_SIZE - scaled_width) / 2;
                int y_offset = (SQUARE_OUTPUT_SIZE - scaled_height) / 2;
                resized.copyTo(squareOutput(cv::Rect(x_offset, y_offset, scaled_width, scaled_height)));
                
                out.write(squareOutput);
            }
            frames_written++;
        }

        // Update rolling index
        rollingIndex = (rollingIndex + 1) % 3;
    }

    // Calculate statistics
    stats.dropped_frames = stats.total_frames - frames_written;
    stats.drop_percentage = (static_cast<double>(stats.dropped_frames) / stats.total_frames) * 100.0;
    stats.processed_duration_sec = frames_written / fps;

    std::cout << "  Frames written: " << frames_written << std::endl;
    std::cout << "  Frames dropped: " << stats.dropped_frames << std::endl;
    std::cout << "  Drop percentage: " << std::fixed << std::setprecision(2) 
              << stats.drop_percentage << "%" << std::endl;

    cap.release();
    out.release();
    
    return stats;
}

int main() {
    std::cout << "Motion-informed video processing pipeline" << std::endl;
    std::cout << "===========================================" << std::endl;

    // Create output folder if it doesn't exist
    if (!fs::exists(OUTPUT_FOLDER)) {
        fs::create_directories(OUTPUT_FOLDER);
        std::cout << "Created output folder: " << OUTPUT_FOLDER << std::endl;
    }

    // Check if input folder exists
    if (!fs::exists(INPUT_FOLDER)) {
        std::cerr << "Error: Input folder does not exist: " << INPUT_FOLDER << std::endl;
        return -1;
    }

    // Open CSV file for results
    std::ofstream csv_file(CSV_OUTPUT);
    if (!csv_file.is_open()) {
        std::cerr << "Error: Could not create CSV file: " << CSV_OUTPUT << std::endl;
        return -1;
    }
    
    writeCSVHeader(csv_file);

    // Process all video files in the input folder
    std::vector<std::string> video_extensions = {".mp4", ".avi", ".mov", ".mkv", ".MP4", ".AVI", ".MOV", ".MKV"};
    std::vector<VideoStats> all_stats;
    
    for (const auto& entry : fs::directory_iterator(INPUT_FOLDER)) {
        if (entry.is_regular_file()) {
            std::string extension = entry.path().extension().string();
            
            // Check if file has a video extension
            if (std::find(video_extensions.begin(), video_extensions.end(), extension) != video_extensions.end()) {
                std::string input_path = entry.path().string();
                std::string output_filename = entry.path().stem().string() + "_processed" + extension;
                std::string output_path = fs::path(OUTPUT_FOLDER) / output_filename;
                
                VideoStats stats = processVideo(input_path, output_path);
                all_stats.push_back(stats);
                
                // Write to CSV
                csv_file << stats.filename << ","
                        << std::fixed << std::setprecision(2) << stats.original_duration_sec << ","
                        << std::fixed << std::setprecision(2) << stats.processed_duration_sec << ","
                        << stats.total_frames << ","
                        << stats.dropped_frames << ","
                        << std::fixed << std::setprecision(2) << stats.drop_percentage << "\n";
                
                std::cout << std::endl;
            }
        }
    }

    csv_file.close();
    
    std::cout << "===========================================" << std::endl;
    std::cout << "Processing complete!" << std::endl;
    std::cout << "Results saved to: " << CSV_OUTPUT << std::endl;
    std::cout << "Processed videos saved to: " << OUTPUT_FOLDER << std::endl;

    return 0;
}
