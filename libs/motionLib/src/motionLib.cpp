#include "motionLib/motionLib.hpp"

namespace motionLib {

cv::Mat preprocessFrame(const cv::Mat& frame, const cv::Size& frameSize, const cv::Size& blurKernel) {
    // Check if kernel size is valid
    if (blurKernel.width % 2 == 0 || blurKernel.height % 2 == 0 || 
        blurKernel.width <= 0 || blurKernel.height <= 0) {
        throw std::runtime_error("Invalid kernel size, must be odd and positive");
    }
    // Check if frame is empty
    if (frame.empty()) {
        throw std::runtime_error("Empty frame");
    }
    // Check if frame is too small, no upscaling is supported at this time
    if (frame.cols < frameSize.width || frame.rows < frameSize.height) {
        throw std::runtime_error("Frame too small, must be at least " + 
                                std::to_string(frameSize.width) + "x" + 
                                std::to_string(frameSize.height) + ", but received " +
                                std::to_string(frame.cols) + "x" +
                                std::to_string(frame.rows));
    }
    
    cv::Mat resized, gray, blurred;

    // Resize frame if not already at target size
    if (frame.cols != frameSize.width || frame.rows != frameSize.height) {
        cv::resize(frame, resized, frameSize);
    } else {
        resized = frame;
    }
    // Convert to grayscale
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
    // Apply Gaussian blur
    cv::GaussianBlur(gray, blurred, blurKernel, 0);
    
    return blurred;
}


cv::Mat amplifyMotion(const cv::Mat& prev, const cv::Mat& curr, const cv::Mat& next, float gamma) {
    // Calculate absolute differences directly
    cv::Mat absDelta1, absDelta2;
    cv::absdiff(curr, prev, absDelta1);
    cv::absdiff(next, curr, absDelta2);
    
    // Sum the absolute differences in-place
    cv::add(absDelta1, absDelta2, absDelta1);
    
    // Find maximum value for normalization
    double maxVal;
    cv::minMaxLoc(absDelta1, nullptr, &maxVal);
    // Important! In the case of no motion, maxVal has a small value
    // and the normalization "pumps up" low values, causing false triggers.
    if (maxVal < 100) maxVal = 100;
    
    // Convert to float, normalize, apply gamma, and scale to 255
    cv::Mat amplified;
    absDelta1.convertTo(amplified, CV_32F, 1.0/maxVal);
    cv::pow(amplified, gamma, amplified);
    amplified.convertTo(amplified, CV_8U, 255.0);
    
    return amplified;
}

bool motionDetection(const cv::Mat& frame, int kernelSize, int threshold) {
    if (frame.rows % kernelSize != 0 || frame.cols % kernelSize != 0) {
        throw std::runtime_error("Frame size not divisible by kernel size");
    }

    // Create integral image, sums will be O(1)
    cv::Mat integralImg;
    cv::integral(frame, integralImg, CV_32S); // 32-bit signed integer

    // Process each non-overlapping kernel
    for (int y = 0; y < frame.rows; y += kernelSize) {
        for (int x = 0; x < frame.cols; x += kernelSize) {
            // For a region (x,y) to (x+w,y+h), the sum is:
            // sum = I(y+h,x+w) - I(y,x+w) - I(y+h,x) + I(y,x)
            int sum = integralImg.at<int>(y + kernelSize, x + kernelSize) - 
                      integralImg.at<int>(y, x + kernelSize) - 
                      integralImg.at<int>(y + kernelSize, x) + 
                      integralImg.at<int>(y, x);
            
            // Check if sum exceeds threshold
            if (sum >= threshold) {
                return true; // Motion detected
            }
        }
    }

    return false; // No significant motion detected
}


std::vector<cv::Mat> filterVideo(const std::vector<cv::Mat>& originalFrames, const std::vector<cv::Mat>& motionFrames, int kernelSize, int threshold) {
    if (motionFrames.empty() || originalFrames.empty()) 
        throw std::runtime_error("Empty frame vector");

    std::vector<cv::Mat> filteredFrames;

    // First, ensure both vectors have the same size (accounting for the first two original frames)
    if (originalFrames.size() - 2 != motionFrames.size()) {
        throw std::runtime_error("Original frames and motion frames must have the same size");
    }

    int droppedFrames = 0;
    for (size_t i = 0; i < motionFrames.size(); i++) {
        if (motionDetection(motionFrames[i], kernelSize, threshold)) {
            filteredFrames.push_back(originalFrames[i+2]);
        } else {
            filteredFrames.push_back(cv::Mat::zeros(originalFrames[i+2].size(), originalFrames[i+2].type()));
            ++droppedFrames;
        }
    }

    // Output frame drop statistics
    std::cout << "Total frames processed: " << motionFrames.size()
              << ", Frame drops: " << droppedFrames
              << ", Drop percentage: " << (100.0 * droppedFrames / motionFrames.size()) << "%" 
              << std::endl;

    return filteredFrames;
}

} // namespace motionLib