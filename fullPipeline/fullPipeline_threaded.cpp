#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

#include "motionLib/motionLib.hpp"

// How many seconds to reconrd and send
const int RECORD_SECONDS = 10;
const int FPS = 30;
const int TOTAL_FRAMES = RECORD_SECONDS * FPS;
const int GAMMA = 3;
// Motion detection hyperparameters
const int KSIZE = 20;
const int THRESHOLD = 2000;
// Where to send the stream
const std::string HOST = "192.168.18.168";
// Queue size limits
const int MAX_QUEUE_SIZE = 10;

// Thread-safe queue implementation
template<typename T>
class SafeQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cond_not_empty;
    std::condition_variable cond_not_full;
    int max_size;

public:
    SafeQueue(int max_size) : max_size(max_size) {}

    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex);
        // Wait until queue is not full
        cond_not_full.wait(lock, [this] { return queue.size() < max_size; });
        
        queue.push(std::move(item));
        
        // Notify that queue is not empty
        cond_not_empty.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex);
        // Wait until queue is not empty
        cond_not_empty.wait(lock, [this] { return !queue.empty(); });
        
        item = std::move(queue.front());
        queue.pop();
        
        // Notify that queue is not full
        cond_not_full.notify_one();
        return true;
    }

    bool try_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }
        item = std::move(queue.front());
        queue.pop();
        cond_not_full.notify_one();
        return true;
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(mutex);
        return queue.empty();
    }

    void clear() {
        std::unique_lock<std::mutex> lock(mutex);
        while (!queue.empty()) {
            queue.pop();
        }
        cond_not_full.notify_all();
    }
};

// Structure for frame data
struct FrameData {
    cv::Mat frame;
    cv::Mat processedFrame;
    bool hasMotion;
};

// Global queues and control flags
SafeQueue<FrameData> captureToProcessQueue(MAX_QUEUE_SIZE);
SafeQueue<FrameData> processToStreamQueue(MAX_QUEUE_SIZE);
std::atomic<bool> shouldTerminate(false);


/**
 * @brief Stage 1: captures frames from the camera and enqueues them for processing.
 * 
 * @param cap Reference to the cv::VideoCapture object used for capturing video frames.
 *
 * This function continuously captures frames from the specified cv::VideoCapture object.
 * In a loop that runs until shouldTerminate flag is set, it does the following:
 * - Retrieves a frame from the video capture device and stores it in a FrameData object.
 * - Checks if the captured frame is empty, logging an error and signaling termination if so.
 * - Moves the valid frame data into the captureToProcessQueue.
 */
void captureThread(cv::VideoCapture& cap) {
    while (!shouldTerminate) {
        FrameData frameData;
        cap >> frameData.frame;
        
        if (frameData.frame.empty()) {
            std::cerr << "Error: Could not capture frame" << std::endl;
            shouldTerminate = true;
            break;
        }

        captureToProcessQueue.push(std::move(frameData));
    }
}

/**
 * @brief Stage 2: processes frames for motion detection and prepares them for streaming.
 *
 * This function dequeues frames from the captureToProcessQueue, processes them to detect motion,
 * and enqueues the results to the processToStreamQueue. It maintains a rolling buffer of 3
 * preprocessed frames to enable motion detection using temporal differences.
 * It runs untill shouldTerminate flag is set.
 * 
 * The processing pipeline includes:
 * - Frame preprocessing
 * - Motion calculation using 3-frame temporal analysis
 * - Motion detection based on the motion data
 * - Queueing processed frames with motion metadata for streaming
 */
void processThread() {
    // Rolling buffer for motion detection
    std::array<cv::Mat, 3> preprocFrames;
    int rollingIndex = 0;
    bool bufferFilled = false;
    
    while (!shouldTerminate) {
        FrameData frameData;
        if (!captureToProcessQueue.pop(frameData)) {
            continue;
        }

        // Preprocess the frame
        preprocFrames[rollingIndex] = motionLib::preprocessFrame(
            frameData.frame,
            cv::Size(320, 320), 
            cv::Size(5, 5)
        );
        
        // Only start motion detection once we have 3 frames
        frameData.hasMotion = false;
        if (bufferFilled) {
            // Process the frames for motion detection
            cv::Mat amplified = motionLib::amplifyMotion(
                preprocFrames[(rollingIndex+1)%3],  // t-2
                preprocFrames[(rollingIndex+2)%3],  // t-1
                preprocFrames[rollingIndex],        // t
                GAMMA
            );
            
            // Detect motion
            frameData.hasMotion = motionLib::motionDetection(amplified, KSIZE, THRESHOLD);
        }
        
        // Update rolling index
        rollingIndex = (rollingIndex + 1) % 3;
        if (rollingIndex == 0) {
            bufferFilled = true;
        }
        
        // Send to streaming thread
        processToStreamQueue.push(std::move(frameData));
    }
}

/**
 * @brief Stage 3: streams frames over the network based on motion detection results.
 * 
 * @param out Reference to the cv::VideoWriter object used for streaming video frames.
 *
 * This function dequeues processed frames from the processToStreamQueue and selectively
 * streams them based on motion detection results. In a loop that runs until shouldTerminate
 * flag is set:
 * - Retrieves processed frame data with motion metadata from the queue
 * - Only writes frames to the video output stream if motion was detected
 * - Implements motion-informed frame dropping to reduce bandwidth usage
 */
void streamThread(cv::VideoWriter& out) {
    while (!shouldTerminate) {
        FrameData frameData;
        if (!processToStreamQueue.pop(frameData)) {
            continue;
        }
        
        // Stream if motion detected
        if (frameData.hasMotion) {
            out.write(frameData.frame);
        }
    }
}

int main() {
    std::cout << "WiFi frame stream with motion informed frame drop" << std::endl;

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
    const int fourcc = cv::VideoWriter::fourcc('Y', 'U', '1', '2');
    // Custom square resolution
    const cv::Size resolution = cv::Size(640, 640); // Square format for AI pipeline
    // Set the camera properties
    cap.set(cv::CAP_PROP_FPS, FPS);
    cap.set(cv::CAP_PROP_FOURCC, fourcc);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, resolution.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);


    // ----------------
    // WIFI STREAM SETUP
    // ----------------
    /*
    GStreamer pipeline for H.264 video streaming over UDP/RTP:
    1. appsrc - Source element that accepts video frames from application code
    2. videoconvert - Converts video format/colorspace as needed for downstream elements
    3. video/x-raw,format=NV12,width=640,height=640,framerate=30/1 - Caps filter specifying:
       - NV12 format (YUV 4:2:0 with interleaved UV plane). Requires minimal conversion in openCV,
       still faster than RGB to produce.
       - 640x640 resolution
       - 30 fps framerate
    4. v4l2h264enc - Hardware H.264 encoder using Video4Linux2 interface with:
       - GOP size of 1 (every frame is a keyframe, increases bandwidth but reduces latency)
       - 0 B-frames (no bidirectional prediction frames, reduces encoding latency)
    5. video/x-h264,level=(string)3.1 - H.264 level 3.1 constraint (supports up to 1280x720@30fps)
    6. h264parse - Parses H.264 stream and adds metadata needed for RTP packetization
    7. rtph264pay - Packetizes H.264 stream into RTP packets with:
       - config-interval=1 (send SPS/PPS with every second for robustness)
       - pt=96 (RTP payload type 96, dynamic payload type for H.264)
    8. application/x-rtp,clock-rate=90000 - RTP caps with 90kHz clock rate (standard for video)
    9. udpsink - Sends RTP packets via UDP to specified host and port 5000
    */
    std::string pipeline =
        "appsrc ! videoconvert "
        "! video/x-raw,format=NV12,width=640,height=640,framerate=30/1 "
        "! v4l2h264enc extra-controls=controls,video_gop_size=1,video_b_frames=0 "
        "! video/x-h264,level=(string)3.1 "
        "! h264parse "
        "! rtph264pay config-interval=1 pt=96 "
        "! application/x-rtp,clock-rate=90000 "
        "! udpsink host=" + HOST + " port=5000";
    cv::VideoWriter out(pipeline, cv::CAP_GSTREAMER, 0, FPS, resolution, true);
    if (!out.isOpened()) {
        std::cerr << "Failed to open video writer." << std::endl;
        return -1;
    }


    // ----------------------------
    // THREADED PIPELINE SETUP
    // ----------------------------
    
    // Start threads
    std::thread capture_t(captureThread, std::ref(cap));
    std::thread process_t(processThread);
    std::thread stream_t(streamThread, std::ref(out));
    
    // Main thread can now do other things or just wait
    std::cout << "Pipeline running. Press Enter to stop..." << std::endl;
    std::cin.get(); // Wait for Enter key
    
    // Signal threads to terminate
    shouldTerminate = true;
    
    // Wait for threads to finish
    capture_t.join();
    process_t.join();
    stream_t.join();
    
    std::cout << "Done!" << std::endl;
    cap.release();

    return 0;
}
