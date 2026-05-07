#pragma once

#include <string>


namespace utils {

/**
 * @brief A simple progress bar class for console output.
 */
class ProgressBar {
    private:
        int total;
        int current;
        int barWidth;
        std::string description;

    public:
        /**
         * @brief Constructor for ProgressBar.
         * @param total Total number of iterations.
         * @param desc Description for the progress bar.
         * @param barWidth Width of the progress bar.
         */
        ProgressBar(int total, const std::string& desc = "", int barWidth = 50);
        
        /**
         * @brief Update the progress bar.
         * @param newProgress Current progress.
         */
        void update(int newProgress);
        
        /**
         * @brief Mark the progress bar as done.
         */
        void done();
};

/**
 * @brief Struct to hold command-line arguments.
 */
struct Arguments {
    std::string videoPath; // The path to the input video file.
    std::string outputPath; // The path to the output video file.
    double gamma; // The gamma value for filtering.
    double seconds; // The number of seconds to process from the video.
    bool hasSeconds;
};


/**
 * @brief Parse command-line arguments.
 * 
 * Parse command-line arguments and write them into the provided
 * Arguments struct. If an argument is not provided, the corresponding
 * field in the Arguments struct will be left unchanged.
 * 
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param args Reference to Arguments struct to fill.
 */
void parseArguments(int argc, char* argv[], Arguments& args);


} // namespace utils
