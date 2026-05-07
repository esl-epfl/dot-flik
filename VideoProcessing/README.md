# Video Processing folder

This folder contains the code for the image processing and motion detection part of the pipeline, in C++.

The video processing pipeline is responsible for processing video streams from cameras or video files, including:
- Image preprocessing: resize, blur, and format conversion
- Motion enhancement: pixel subtraction and delta optimization algorithms
- Movement detection: block-based analysis to detect motion presence using configurable thresholds
- Performance profiling and hyperparameter tuning capabilities
- Video filtering based on motion detection results

This module focuses specifically on the computer vision and signal processing aspects and does not include:
- Camera stream acquisition
- Network streaming and Wi-Fi transmission

## Components

- `VideoProcessing.cpp`: Main video processing application that replaces camera input with video files and outputs processed results to video files. Implements the complete motion detection pipeline with configurable parameters for testing and development.

- `HyperpTuning.cpp`: Hyperparameter optimization tool that processes video files with different parameter combinations to find optimal motion detection settings. Generates CSV reports with performance metrics and detection accuracy results.

- `VideoFiltering.cpp`: Motion-based video filter that processes input videos and outputs new video files containing only frames with sufficient motion activity.

- `StressTest.cpp`: Performance benchmarking tool that measures the computational efficiency of the motion detection pipeline. Uses randomly selected frames to test processing speed and provides average timing statistics. Note that results may be optimistic due to memory caching effects.

## Pipeline Architecture

The motion detection pipeline follows these key stages:

1. **Frame Preprocessing**: Resize frames to target resolution (default: 640x640) and apply Gaussian blur filtering
2. **Motion Enhancement**: Calculate frame differences and apply delta optimization techniques
3. **Block-based Detection**: Divide frames into configurable blocks and analyze motion within each region
4. **Threshold Comparison**: Compare motion intensity against configurable thresholds to determine motion presence
5. **Result Processing**: Output filtered frames, statistics, or performance metrics based on the specific application

## Configuration

All applications support command-line arguments for:
- Input video path specification
- Output path configuration
- Motion detection sensitivity parameters
- Frame processing options
- Performance profiling settings

---

→ Back to root [README](../README.md)