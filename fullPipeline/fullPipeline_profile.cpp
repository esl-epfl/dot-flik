#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <numeric>
#include <iomanip>
#include <fstream>

#include "motionLib/motionLib.hpp"

// How many seconds to record and send
const int GAMMA = 3;
// Motion detection hyperparameters
const int KSIZE = 20;
const int THRESHOLD = 2000;

// Performance tracking structures
struct PerformanceStats {
    // Running statistics for each metric
    struct RunningStats {
        double sum = 0.0;
        double sum_squared = 0.0;
        double min_val = std::numeric_limits<double>::max();
        double max_val = std::numeric_limits<double>::lowest();
        int count = 0;
        
        void add(double value) {
            sum += value;
            sum_squared += value * value;
            min_val = std::min(min_val, value);
            max_val = std::max(max_val, value);
            count++;
        }
        
        double mean() const {
            return count > 0 ? sum / count : 0.0;
        }
        
        double stddev() const {
            if (count <= 1) return 0.0;
            double variance = (sum_squared - (sum * sum) / count) / (count - 1);
            return std::sqrt(std::max(0.0, variance));
        }
    };
    
    RunningStats camera_stats;
    RunningStats processing_stats;
    RunningStats streaming_stats;
    RunningStats total_stats;
    
    int frames_sent = 0;
    int frames_dropped = 0;
    
    void printStats() const {
        std::cout << "\n=== PERFORMANCE STATISTICS ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        
        std::cout << "Camera Capture:   " << camera_stats.mean() << "ms ± " << camera_stats.stddev() 
                  << " (min: " << camera_stats.min_val << ", max: " << camera_stats.max_val << ")" << std::endl;
        std::cout << "Motion Processing: " << processing_stats.mean() << "ms ± " << processing_stats.stddev()
                  << " (min: " << processing_stats.min_val << ", max: " << processing_stats.max_val << ")" << std::endl;
        std::cout << "Streaming:        " << streaming_stats.mean() << "ms ± " << streaming_stats.stddev()
                  << " (min: " << streaming_stats.min_val << ", max: " << streaming_stats.max_val << ")" << std::endl;
        std::cout << "Total per frame:  " << total_stats.mean() << "ms ± " << total_stats.stddev()
                  << " (min: " << total_stats.min_val << ", max: " << total_stats.max_val << ")" << std::endl;
        
        std::cout << "Frames processed: " << total_stats.count << std::endl;
        std::cout << "Frames sent:      " << frames_sent << std::endl;
        std::cout << "Frames dropped:   " << frames_dropped << std::endl;
        std::cout << "Drop rate:        " << (frames_dropped * 100.0 / (frames_sent + frames_dropped)) << "%" << std::endl;
        std::cout << "Effective FPS:    " << (1000.0 / total_stats.mean()) << std::endl;

        std::cout << "Total time:       " << total_stats.sum/1000.0 << "s" << std::endl; 
    }
    
    void saveToCSV(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << filename << " for writing" << std::endl;
            return;
        }
        
        // CSV Header with summary statistics
        file << "metric,mean_ms,stddev_ms,min_ms,max_ms,count\n";
        
        // CSV Data
        file << std::fixed << std::setprecision(3);
        file << "camera_capture," << camera_stats.mean() << "," << camera_stats.stddev() 
             << "," << camera_stats.min_val << "," << camera_stats.max_val << "," << camera_stats.count << "\n";
        file << "motion_processing," << processing_stats.mean() << "," << processing_stats.stddev()
             << "," << processing_stats.min_val << "," << processing_stats.max_val << "," << processing_stats.count << "\n";
        file << "streaming," << streaming_stats.mean() << "," << streaming_stats.stddev()
             << "," << streaming_stats.min_val << "," << streaming_stats.max_val << "," << streaming_stats.count << "\n";
        file << "total_frame," << total_stats.mean() << "," << total_stats.stddev()
             << "," << total_stats.min_val << "," << total_stats.max_val << "," << total_stats.count << "\n";
        
        // Additional summary row
        file << "\n# Summary\n";
        file << "frames_sent," << frames_sent << "\n";
        file << "frames_dropped," << frames_dropped << "\n";
        file << "drop_rate_percent," << (frames_dropped * 100.0 / (frames_sent + frames_dropped)) << "\n";
        file << "effective_fps," << (1000.0 / total_stats.mean()) << "\n";
        
        file.close();
        std::cout << "Performance summary saved to: " << filename << std::endl;
    }
};

int main(int argc, char* argv[]) {
    // Parse command line arguments for threading mode, test duration, FPS, host, and port
    bool use_single_thread = false;
    int record_seconds = 30;  // default duration is 30 seconds
    int fps = 30;             // default FPS is 30
    std::string host = "192.168.18.168"; // default host
    std::string port = "5000";           // default port
    int drop_percentage = 0;  // default: no frames dropped (0%)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--single-thread" || arg == "-s") {
            use_single_thread = true;
        } else if (arg == "--duration" || arg == "-d") {
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
        } else if (arg == "--drop" || arg == "-D") {
            if (i + 1 < argc) {
                drop_percentage = std::stoi(argv[++i]);
                if (drop_percentage < 0 || drop_percentage > 100) {
                    std::cerr << "Error: Drop percentage must be between 0 and 100" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: Missing value for --drop" << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0]
                      << " [--single-thread|-s] [--duration|-d <seconds>] [--fps|-f <frames per second>] "
                      << "[--host|-H <host address>] [--port|-P <port number>] [--drop|-D <percentage>] [--help|-h]" << std::endl;
            std::cout << "  --single-thread, -s: Use single thread for OpenCV operations" << std::endl;
            std::cout << "  --duration, -d: Set test duration in seconds (default: 30)" << std::endl;
            std::cout << "  --fps, -f: Set frames per second (default: 30)" << std::endl;
            std::cout << "  --host, -H: Set the host address (default: 192.168.18.168)" << std::endl;
            std::cout << "  --port, -P: Set the port number (default: 5000)" << std::endl;
            std::cout << "  --drop, -D: Set percentage of frames to drop (0-100, default: 0)" << std::endl;
            std::cout << "  --help, -h: Show this help message" << std::endl;
            return 0;
        }
    }
    int total_frames = record_seconds * fps;
    
    /* 
    cv::setNumThreads() NOT WORKING
    WORKAROUND: disable all the cores but one
    'maxcpus=1' in /boot/cmdline.txt as first element of the line
    */
    // Configure OpenCV threading
    // if (use_single_thread) {
    //     cv::setNumThreads(1);
    //     std::cout << "Running in SINGLE-THREADED mode (1 thread)" << std::endl;
    // } else {
    //     cv::setNumThreads(0); // Use all available threads
    //     std::cout << "Running in MULTI-THREADED mode (" << cv::getNumThreads() << " threads)" << std::endl;
    // }
    
    std::cout << "WiFi frame stream with motion informed frame drop - PROFILING MODE" << std::endl;
    std::cout << "Running for: " + std::to_string(record_seconds) + "s" << std::endl;
    std::cout << "Running with " << cv::getNumThreads() << " threads" << std::endl;
    std::cout << "Frame drop percentage: " << drop_percentage << "%" << std::endl;
    
    PerformanceStats stats;

    // --------------
    // CAMERA SETUP
    // --------------

    /*
    GStreamer pipeline for camera capture using V4L2:
    1. v4l2src device=/dev/video0 - V4L2 source element capturing from camera device
    2. video/x-raw,format=YU12,width=640,height=640,framerate=30/1 - Caps filter specifying:
       - NV12 format (Planar YUV format, same as before)
       - 640x640 resolution (square format for AI pipeline)
       - 30 fps framerate (or user-specified fps)
    3. videoconvert - Converts video format as needed for OpenCV
    4. appsink - Sink element that provides frames to application code
    */
    std::string camera_pipeline = 
        "v4l2src device=/dev/video0 "
        "! video/x-raw,format=NV12,width=640,height=640,framerate=" + std::to_string(fps) + "/1 "
        "! videoconvert "
        "! video/x-raw,format=BGR,width=640,height=640"
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
    // MOTION-INFORMED FRAME DROP LOOP
    // ----------------------------
    // Rolling buffers
    std::array<cv::Mat, 3> frames;
    std::array<cv::Mat, 3> preprocFrames;
    int rollingIndex = 0;
    // Allocate variables
    cv::Mat amplified;

    // Fill rolling buffer (not profiled - just initialization)
    for (int initialIndex = 0; initialIndex < 3; initialIndex++) {
        cap >> frames[initialIndex];
        
        if (frames[initialIndex].empty()) {
            std::cerr << "Error: Could not capture frame" << std::endl;
            break;
        }

        preprocFrames[initialIndex] = motionLib::preprocessFrame(
            frames[initialIndex],
            cv::Size(320, 320), 
            cv::Size(5, 5)
        );
    }

    // Main loop body
    for (int f = 0; f < total_frames; f++) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        // Get the frame from camera
        auto camera_start = std::chrono::high_resolution_clock::now();
        cap >> frames[rollingIndex];
        auto camera_end = std::chrono::high_resolution_clock::now();
        auto camera_duration = std::chrono::duration_cast<std::chrono::microseconds>(camera_end - camera_start);
        stats.camera_stats.add(camera_duration.count() / 1000.0);
        
        if (frames[rollingIndex].empty()) {
            std::cerr << "Error: Could not capture frame" << std::endl;
            break;
        }

        // Motion processing timing
        auto processing_start = std::chrono::high_resolution_clock::now();
        
        // Resize it to 320x320 and preprocess with 5x5 blur kernel
        preprocFrames[rollingIndex] = motionLib::preprocessFrame(
            frames[rollingIndex],
            cv::Size(320, 320), 
            cv::Size(5, 5)
        );

        // Process the frames...
        // (i+1)%3 - (i+2)%3 - i
        amplified = motionLib::amplifyMotion(
            preprocFrames[(rollingIndex+1)%3],  // t-2
            preprocFrames[(rollingIndex+2)%3],  // t-1
            preprocFrames[rollingIndex],        // t
            GAMMA
        );

        // Motion detection for profiling (result not used for frame dropping - using drop percentage instead)
        bool motion_detected = motionLib::motionDetection(amplified, KSIZE, THRESHOLD);
        
        auto processing_end = std::chrono::high_resolution_clock::now();
        auto processing_duration = std::chrono::duration_cast<std::chrono::microseconds>(processing_end - processing_start);
        stats.processing_stats.add(processing_duration.count() / 1000.0);

        // Streaming timing
        auto streaming_start = std::chrono::high_resolution_clock::now();
        double streaming_time = 0.0;
        
        // Determine if frame should be sent based on drop percentage
        bool should_send_frame = true;
        int rollingIndex = 0;
        if (drop_percentage > 0) {
            // Use modulo-based dropping to achieve the desired percentage
            // For example: 10% drop = send 9 out of every 10 frames
            int send_rate = 100 - drop_percentage;  // percentage of frames to send
            should_send_frame = rollingIndex < send_rate;
            rollingIndex = ++rollingIndex % 100;
        }
        
        // Send original frame to network IF not dropped
        if (should_send_frame) {
            out.write(frames[rollingIndex]);
            
            auto streaming_end = std::chrono::high_resolution_clock::now();
            auto streaming_duration = std::chrono::duration_cast<std::chrono::microseconds>(streaming_end - streaming_start);
            stats.frames_sent++;
            streaming_time = streaming_duration.count() / 1000.0;
        } else {
            stats.frames_dropped++;
        }
        stats.streaming_stats.add(streaming_time);

        auto frame_end = std::chrono::high_resolution_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
        stats.total_stats.add(frame_duration.count() / 1000.0);

        // Update rolling index
        rollingIndex = (rollingIndex + 1) % 3;
    }

    // Print final statistics and save CSV
    stats.printStats();
    
    // Generate CSV filename with threading mode
    std::string csv_filename = use_single_thread ? "performance_single_thread.csv" : "performance_multi_thread.csv";
    stats.saveToCSV(csv_filename);

    std::cout << "\nDone!" << std::endl;
    cap.release();

    return 0;
}
