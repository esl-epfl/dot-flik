cmake_minimum_required(VERSION 3.14)
project(MattiasThesis)

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies
find_package(OpenCV REQUIRED)

# Create global compiler settings interface
add_library(compilerFlags INTERFACE)
target_compile_features(compilerFlags INTERFACE cxx_std_17)
target_compile_options(compilerFlags INTERFACE
    -Wall -Wextra
    -g
    -O3
    # -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard
)

# Add custom libraries
add_subdirectory(libs)
# Add executables
add_subdirectory(VideoProcessing)
add_subdirectory(CameraStreamer)
add_subdirectory(fullPipeline)
