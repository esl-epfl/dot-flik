#pragma once

#include <opencv2/opencv.hpp>

namespace motionLib {

/**
 * @brief Detects motion in a single channel frame
 *
 * This function divides the input frame into blocks of size kernelSize × kernelSize and
 * calculates the sum of pixel values in each block. If any block's sum exceeds the specified
 * threshold, the function returns true, indicating motion detection.
 *
 * @param frame Grayscale input frame to be analyzed for motion
 * @param kernelSize Size of the square blocks used for analysis (default: 5)
 * @param threshold Minimum sum value to consider motion detected (default: 0)
 * 
 * @return true if motion is detected in any block, false otherwise
 * 
 * @throws std::runtime_error if frame dimensions are not divisible by kernelSize
 */
bool motionDetection(const cv::Mat& frame, int kernelSize = 5, int threshold = 0);


/**
 * @brief Calculate and amplify motion between consecutive frames.
 * 
 * This function detects motion by calculating the absolute differences between
 * consecutive frames (prev-curr and curr-next), combining them, and then
 * enhancing the result using gamma correction. The output is a grayscale image
 * where brighter pixels represent areas with more motion.
 * 
 * @param prev The previous video frame (t-1)
 * @param curr The current video frame (t)
 * @param next The next video frame (t+1)
 * @param gamma Gamma correction factor to control motion amplification intensity (default: 2.5)
 *              Higher values produce more contrast in motion areas
 * 
 * @return cv::Mat Motion map as an 8-bit grayscale image where pixel intensity represents motion magnitude
 *         Returns a black image if no motion is detected
 */
cv::Mat amplifyMotion(const cv::Mat& prev, const cv::Mat& curr, const cv::Mat& next, float gamma = 2.5);


/**
 * @brief Resizing, Convert to grayscale, and apply Gaussian blur to frame.
 * 
 * This function takes an input frame, resizes it to the specified dimensions, converts it to a 
 * grayscale image, and applies a Gaussian blur with the specified kernel size.
 * 
 * @param frame The input frame as a cv::Mat object. Must be at least as large as the specified frameSize.
 * @param frameSize The target size to which the frame will be resized (width, height).
 * @param blurKernel The size of the kernel to be used for Gaussian blur (width, height). Must be odd and positive.
 * 
 * @return A cv::Mat object containing the preprocessed frame.
 * 
 * @throws std::runtime_error If input frame is empty, kernel size is invalid, or frame is too small.
 */
cv::Mat preprocessFrame(const cv::Mat& frame, const cv::Size& frameSize, const cv::Size& blurKernel);

/**
 * @brief Filter video frames based on motion detection
 * 
 * Take a vector of video frames and drops all the frames that do not
 * contain significant motion. motionDetection() is used to determine if a frame contains
 * significant motion.
 * 
 * @param originalFrames Vector of cv::Mat frames to be filtered
 * @param motionFrames Vector of cv::Mat frames used for the filtering process
 * @param kernelSize Size of the square blocks used for analysis
 * @param threshold Minimum sum value to consider motion detected
 * 
 * @return A vector of cv::Mat frames that contain significant motion
 * 
 * @throws std::runtime_error if the input frame vector is empty
 */
std::vector<cv::Mat> filterVideo(const std::vector<cv::Mat>& originalFrames, const std::vector<cv::Mat>& motionFrames, int kernelSize, int threshold);

} // namespace motionLib
