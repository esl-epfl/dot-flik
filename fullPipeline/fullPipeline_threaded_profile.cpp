#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <numeric>
#include <iomanip>
#include <fstream>

#include "motionLib/motionLib.hpp"

// Default values for motion detection hyperparameters
const int GAMMA = 3;
// Motion detection hyperparameters
const int KSIZE = 20;
const int THRESHOLD = 2000;
// Where to send the stream
const std::string HOST = "192.168.18.168";
// Queue size limits
const int MAX_QUEUE_SIZE = 10;

// Make the definition shorter
typedef std::chrono::high_resolution_clock::time_point chronoTime;

// Performance tracking structures
struct PerformanceStats {
    // Simple counters for throughput measurement
    std::atomic<int> frames_captured{0};
    std::atomic<int> frames_processed{0};
    std::atomic<int> frames_sent{0};
    std::atomic<int> frames_dropped{0};
    
    // Timing for pipeline start/end
    std::chrono::high_resolution_clock::time_point pipeline_start;
    std::chrono::high_resolution_clock::time_point pipeline_end;
    
    // Track actual processing times (not including queue waits)
    struct RunningStats {
        double sum = 0.0;
        double sum_squared = 0.0;
        double min_val = std::numeric_limits<double>::max();
        double max_val = std::numeric_limits<double>::lowest();
        int count = 0;
        mutable std::mutex mutex;
        
        void add(double value) {
            std::lock_guard<std::mutex> lock(mutex);
            sum += value;
            sum_squared += value * value;
            min_val = std::min(min_val, value);
            max_val = std::max(max_val, value);
            count++;
        }
        
        double mean() const {
            std::lock_guard<std::mutex> lock(mutex);
            return count > 0 ? sum / count : 0.0;
        }
        
        double stddev() const {
            std::lock_guard<std::mutex> lock(mutex);
            if (count <= 1) return 0.0;
            double variance = (sum_squared - (sum * sum) / count) / (count - 1);
            return std::sqrt(std::max(0.0, variance));
        }
        
        double getMin() const {
            std::lock_guard<std::mutex> lock(mutex);
            return min_val;
        }
        
        double getMax() const {
            std::lock_guard<std::mutex> lock(mutex);
            return max_val;
        }
        
        int getCount() const {
            std::lock_guard<std::mutex> lock(mutex);
            return count;
        }
    };
    
    RunningStats motion_processing_stats;  // Only actual motion processing time
    RunningStats streaming_stats;          // Only actual streaming time (when frame is sent)
    
    void startPipeline() {
        pipeline_start = std::chrono::high_resolution_clock::now();
    }
    
    void endPipeline() {
        pipeline_end = std::chrono::high_resolution_clock::now();
    }
    
    void printStats() const {
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            pipeline_end - pipeline_start);
        double total_time_sec = total_duration.count() / 1000.0;
        
        std::cout << "\n=== THROUGHPUT PERFORMANCE STATISTICS ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        
        std::cout << "=== PIPELINE THROUGHPUT ===" << std::endl;
        std::cout << "Total pipeline time:  " << total_time_sec << "s" << std::endl;
        std::cout << "Frames captured:      " << frames_captured.load() << std::endl;
        std::cout << "Frames processed:     " << frames_processed.load() << std::endl;
        std::cout << "Frames sent:          " << frames_sent.load() << std::endl;
        std::cout << "Frames dropped:       " << frames_dropped.load() << std::endl;
        
        // Throughput calculations
        if (total_time_sec > 0) {
            std::cout << "Capture throughput:   " << (frames_captured.load() / total_time_sec) << " fps" << std::endl;
            std::cout << "Processing throughput: " << (frames_processed.load() / total_time_sec) << " fps" << std::endl;
            std::cout << "Streaming throughput: " << (frames_sent.load() / total_time_sec) << " fps" << std::endl;
        }
        
        // Drop rate
        int total_decision_frames = frames_sent.load() + frames_dropped.load();
        if (total_decision_frames > 0) {
            std::cout << "Drop rate:            " << (frames_dropped.load() * 100.0 / total_decision_frames) << "%" << std::endl;
        }
        
        std::cout << "\n=== PROCESSING TIME STATISTICS ===" << std::endl;
        std::cout << "Motion Processing:    " << motion_processing_stats.mean() << "ms ± " << motion_processing_stats.stddev()
                  << " (min: " << motion_processing_stats.getMin() << ", max: " << motion_processing_stats.getMax() 
                  << ", count: " << motion_processing_stats.getCount() << ")" << std::endl;
        std::cout << "Streaming (when sent): " << streaming_stats.mean() << "ms ± " << streaming_stats.stddev()
                  << " (min: " << streaming_stats.getMin() << ", max: " << streaming_stats.getMax()
                  << ", count: " << streaming_stats.getCount() << ")" << std::endl;
    }
    
    void saveToCSV(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << filename << " for writing" << std::endl;
            return;
        }
        
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            pipeline_end - pipeline_start);
        double total_time_sec = total_duration.count() / 1000.0;
        
        // CSV Header
        file << "metric,value,unit\n";
        
        // Throughput data
        file << std::fixed << std::setprecision(3);
        file << "total_time," << total_time_sec << ",seconds\n";
        file << "frames_captured," << frames_captured.load() << ",count\n";
        file << "frames_processed," << frames_processed.load() << ",count\n";
        file << "frames_sent," << frames_sent.load() << ",count\n";
        file << "frames_dropped," << frames_dropped.load() << ",count\n";
        
        if (total_time_sec > 0) {
            file << "capture_throughput," << (frames_captured.load() / total_time_sec) << ",fps\n";
            file << "processing_throughput," << (frames_processed.load() / total_time_sec) << ",fps\n";
            file << "streaming_throughput," << (frames_sent.load() / total_time_sec) << ",fps\n";
        }
        
        int total_decision_frames = frames_sent.load() + frames_dropped.load();
        if (total_decision_frames > 0) {
            file << "drop_rate_percent," << (frames_dropped.load() * 100.0 / total_decision_frames) << ",percent\n";
        }
        
        // Processing time statistics
        file << "motion_processing_mean," << motion_processing_stats.mean() << ",ms\n";
        file << "motion_processing_stddev," << motion_processing_stats.stddev() << ",ms\n";
        file << "motion_processing_min," << motion_processing_stats.getMin() << ",ms\n";
        file << "motion_processing_max," << motion_processing_stats.getMax() << ",ms\n";
        file << "streaming_mean," << streaming_stats.mean() << ",ms\n";
        file << "streaming_stddev," << streaming_stats.stddev() << ",ms\n";
        file << "streaming_min," << streaming_stats.getMin() << ",ms\n";
        file << "streaming_max," << streaming_stats.getMax() << ",ms\n";
        
        file.close();
        std::cout << "Performance summary saved to: " << filename << std::endl;
    }
};

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
        cond_not_full.wait(lock, [this] { return queue.size() < static_cast<size_t>(max_size); });
        
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

// Structure for frame data - simplified for throughput measurement
struct FrameData {
    cv::Mat frame;
    bool hasMotion = false;
};

// Global queues and control flags
SafeQueue<FrameData> captureToProcessQueue(MAX_QUEUE_SIZE);
SafeQueue<FrameData> processToStreamQueue(MAX_QUEUE_SIZE);
std::atomic<bool> shouldTerminate(false);
PerformanceStats globalStats;


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

        // Simply count captured frames - no timing needed as it's blocking at target FPS
        globalStats.frames_captured++;
        
        // Push to processing queue
        captureToProcessQueue.push(std::move(frameData));
    }
}

/**
 * @brief Stage 2: processes frames for motion detection and prepares them for streaming.
 *
 * This function dequeues frames from the captureToProcessQueue, processes them to detect motion,
 * and enqueues the results to the processToStreamQueue. It maintains a rolling buffer of 3
 * preprocessed frames to enable motion detection using temporal differences.
 * It runs until shouldTerminate flag is set.
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

        // Time only the actual motion processing work
        chronoTime processing_start = std::chrono::high_resolution_clock::now();

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
        
        chronoTime processing_end = std::chrono::high_resolution_clock::now();
        
        // Record only actual processing time (exclude queue operations)
        auto processing_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            processing_end - processing_start);
        globalStats.motion_processing_stats.add(processing_duration.count() / 1000.0);
        globalStats.frames_processed++;
        
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
        frameData.hasMotion = true; // PROFILE: worst case scenario, I transmit all
        if (frameData.hasMotion) {
            chronoTime streaming_start = std::chrono::high_resolution_clock::now();
            out.write(frameData.frame);
            chronoTime streaming_end = std::chrono::high_resolution_clock::now();
            
            auto streaming_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                streaming_end - streaming_start);
            globalStats.streaming_stats.add(streaming_duration.count() / 1000.0);
            globalStats.frames_sent++;
        } else {
            globalStats.frames_dropped++;
        }
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments for test duration, FPS, host, and port
    int record_seconds = 30;  // default duration is 30 seconds
    int fps = 30;             // default FPS is 30
    std::string host = "192.168.18.168"; // default host
    std::string port = "5000";           // default port
    int num_threads = 1;      // default number of processing threads
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--duration" || arg == "-d") {
            if (i + 1 < argc) {
                record_seconds = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: Missing value for --duration" << std::endl;
                return 1;
            }
        } else if (arg == "--fps" || arg == "-f") {
            if (i + 1 < argc) {
                fps = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: Missing value for --fps" << std::endl;
                return 1;
            }
        } else if (arg == "--host" || arg == "-H") {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                std::cerr << "Error: Missing value for --host" << std::endl;
                return 1;
            }
        } else if (arg == "--port" || arg == "-P") {
            if (i + 1 < argc) {
                port = argv[++i];
            } else {
                std::cerr << "Error: Missing value for --port" << std::endl;
                return 1;
            }
        } else if (arg == "--threads" || arg == "-t") {
            if (i + 1 < argc) {
                num_threads = std::stoi(argv[++i]);
                if (num_threads < 1 || num_threads > 8) {
                    std::cerr << "Error: Number of threads must be between 1 and 8" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: Missing value for --threads" << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0]
                      << " [--duration|-d <seconds>] [--fps|-f <frames per second>] "
                      << "[--host|-H <host address>] [--port|-P <port number>] "
                      << "[--threads|-t <num threads>] [--help|-h]" << std::endl;
            std::cout << "  --duration, -d: Set test duration in seconds (default: 30)" << std::endl;
            std::cout << "  --fps, -f: Set frames per second (default: 30)" << std::endl;
            std::cout << "  --host, -H: Set the host address (default: 192.168.18.168)" << std::endl;
            std::cout << "  --port, -P: Set the port number (default: 5000)" << std::endl;
            std::cout << "  --threads, -t: Set number of OpenCV threads (default: 1, max: 4)" << std::endl;
            std::cout << "  --help, -h: Show this help message" << std::endl;
            return 0;
        }
    }
    
    std::cout << "WiFi frame stream with motion informed frame drop - THREADED PROFILING MODE" << std::endl;
    std::cout << "Running for: " + std::to_string(record_seconds) + "s" << std::endl;
    
    // Set OpenCV thread count
    cv::setNumThreads(num_threads);
    std::cout << "Using " << cv::getNumThreads() << " OpenCV threads" << std::endl;

    // --------------
    // CAMERA SETUP
    // --------------

    /*
    GStreamer pipeline for camera capture using V4L2:
    1. v4l2src device=/dev/video0 - V4L2 source element capturing from camera device
    2. video/x-raw,format=NV12,width=640,height=640,framerate=30/1 - Caps filter specifying:
       - NV12 format (YUV 4:2:0 with interleaved UV plane)
       - 640x640 resolution (square format for AI pipeline)
       - 30 fps framerate (or user-specified fps)
    3. videoconvert - Converts video format as needed for OpenCV
    4. video/x-raw,format=BGR,width=640,height=640 - Convert to BGR for OpenCV compatibility
    5. appsink - Sink element that provides frames to application code
    */
    std::string camera_pipeline = 
        "v4l2src device=/dev/video0 "
        "! video/x-raw,format=NV12,width=640,height=640,framerate=" + std::to_string(fps) + "/1 "
        "! videoconvert "
        "! video/x-raw,format=BGR,width=640,height=640 "
        "! appsink";
    
    cv::VideoCapture cap(camera_pipeline, cv::CAP_GSTREAMER);
    // Check if camera opened successfully
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera with GStreamer pipeline" << std::endl;
        std::cerr << "Pipeline: " << camera_pipeline << std::endl;
        return -1;
    }

    // Custom square resolution (already set in pipeline, but store for reference)
    const cv::Size resolution = cv::Size(640, 640); // Square format for AI pipeline


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
        "! video/x-raw,format=NV12,width=640,height=640,framerate=" + std::to_string(fps) + "/1 "
        "! v4l2h264enc extra-controls=controls,video_gop_size=1,video_b_frames=0 "
        "! video/x-h264,level=(string)3.1 "
        "! h264parse "
        "! rtph264pay config-interval=1 pt=96 "
        "! application/x-rtp,clock-rate=90000 "
        "! udpsink host=" + host + " port=" + port;
    cv::VideoWriter out(pipeline, cv::CAP_GSTREAMER, 0, fps, resolution, true);
    if (!out.isOpened()) {
        std::cerr << "Failed to open video writer." << std::endl;
        return -1;
    }


    // ----------------------------
    // THREADED PIPELINE SETUP
    // ----------------------------
    
    std::cout << "Starting threaded pipeline..." << std::endl;
    
    // Start pipeline timing
    globalStats.startPipeline();
    
    // Start threads
    std::thread capture_t(captureThread, std::ref(cap));
    std::thread process_t(processThread);
    std::thread stream_t(streamThread, std::ref(out));
    
    // Main thread waits for the specified duration
    std::cout << "Pipeline running for " << record_seconds << " seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(record_seconds));
    
    // Signal threads to terminate
    std::cout << "Stopping pipeline..." << std::endl;
    shouldTerminate = true;
    
    // Clear queues to unblock waiting threads
    captureToProcessQueue.clear();
    processToStreamQueue.clear();
    
    // Wait for threads to finish
    capture_t.join();
    process_t.join();
    stream_t.join();
    
    // End pipeline timing
    globalStats.endPipeline();
    
    // Print final statistics and save CSV
    globalStats.printStats();
    
    // Generate CSV filename for threaded mode
    std::string csv_filename = "performance_threaded_throughput.csv";
    globalStats.saveToCSV(csv_filename);
    
    std::cout << "\nDone!" << std::endl;
    cap.release();

    return 0;
}
