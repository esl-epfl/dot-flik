#include "utils/utils.hpp"
#include <iostream>
#include <getopt.h>

namespace utils {

ProgressBar::ProgressBar(int total, const std::string& desc, int barWidth) 
    : total(total), current(0), barWidth(barWidth), description(desc) {}

void ProgressBar::update(int newProgress) {
    current = newProgress;
    float progress = static_cast<float>(current) / total;
    int pos = static_cast<int>(barWidth * progress);
    
    std::cout << description;
    if (!description.empty()) std::cout << ": ";
    
    std::cout << "[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << "% " << current << "/" << total << "\r";
    std::cout.flush();
}

void ProgressBar::done() {
    update(total);
    std::cout << std::endl;
}


void parseArguments(int argc, char* argv[], Arguments& args) {
    
    const char* const short_opts = "v:o:g:s:h";
    const option long_opts[] = {
        {"video", required_argument, nullptr, 'v'},
        {"output", required_argument, nullptr, 'o'},
        {"gamma", required_argument, nullptr, 'g'},
        {"seconds", required_argument, nullptr, 's'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };
    
    int opt = 0;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'v':
                args.videoPath = optarg;
                break;
            case 'o':
                args.outputPath = optarg;
                break;
            case 'g':
                args.gamma = std::stod(optarg);
                break;
            case 's':
                args.seconds = std::stod(optarg);
                args.hasSeconds = true;
                break;
            case 'h':
            case '?':
            default:
                std::cout << "Usage: " << argv[0] << " [options]\n"
                          << "Options:\n"
                          << "  -v, --video=PATH      Path to input video file\n"
                          << "  -o, --output=PATH     Path to output video file\n"
                          << "  -g, --gamma=VALUE     Gamma value for amplification (default: 3)\n"
                          << "  -s, --seconds=VALUE   Number of seconds to process (optional)\n"
                          << "  -h, --help            Show this help message\n";
                exit(opt == 'h' ? 0 : 1);
        }
    }
}


} // namespace utils