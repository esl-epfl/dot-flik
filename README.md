# Dot-Flik
## A Scalable Edge AI Architecture for Distributed Insect Monitoring

This repository contains the reference C++ implementation of the *Dot* edge sensing node from the Dot-Flik architecture: a low-cost, energy-efficient platform that performs motion-informed frame filtering at the source and forwards only candidate insect observations to the central *Flik* classification node. The code is optimized for deployment on the Raspberry Pi Zero 2 W.

## Structure of the repository

### CameraStreamer
This folder contains the code for camera stream acquisition and hardware interfacing. It includes camera initialization, video format negotiation, frame capture optimization, and streaming capabilities. The implementation supports the V4L2 backend and is tested on the Raspberry Pi Camera Module V2. Refer to the subfolder [README](./CameraStreamer/README.md) for more details.

### VideoProcessing
This folder contains the code for the image processing and motion detection pipeline. It includes image preprocessing, motion enhancement algorithms, and movement detection. Multiple implementations are provided for development and hyperparameter tuning. Refer to the subfolder [README](./VideoProcessing/README.md) for more details.

### fullPipeline
This folder contains the complete end-to-end pipeline implementations that integrate all system components. It includes basic and threaded versions of the motion-informed streaming pipeline, with performance profiling capabilities and configurable frame dropping for bandwidth optimization. Refer to the subfolder [README](./fullPipeline/README.md) for more details.

### libs
This folder contains the custom libraries used throughout the project:
- `motionLib/`: Core motion detection and image processing algorithms
- `utils/`: Utility functions and helper classes for performance monitoring, data structures, and common operations

The libraries are organized with `include/` and `src/` directory structures and can be built independently using CMake.

## Building the Project

This project uses CMake as its build system. To build the entire project, create a build directory and use CMake:

```bash
mkdir build
cd build
cmake ..
make
```

The compiled executables will be generated in the `build/` directory, organized by their respective components (CameraStreamer, VideoProcessing, fullPipeline).

## Dependencies

- OpenCV (computer vision and video processing) built with the following components:
  - OpenMP
  - GStreamer
  - V4L2
- Custom libraries:
  - `motionLib`
  - `utils`

### OpenCV Build Configuration

The following CMake configuration was used to build OpenCV:

```bash
cmake -D CMAKE_BUILD_TYPE=RELEASE \
-D CMAKE_INSTALL_PREFIX=/usr/local \
-D WITH_OPENCL=OFF \
-D WITH_AVFOUNDATION=OFF \
-D WITH_CAP_IOS=OFF \
-D WITH_CAROTENE=OFF \
-D WITH_CPUFEATURES=OFF \
-D WITH_EIGEN=OFF \
-D WITH_GSTREAMER=ON \
-D WITH_GTK=OFF \
-D WITH_IPP=OFF \
-D WITH_HALIDE=OFF \
-D WITH_VULKAN=OFF \
-D WITH_INF_ENGINE=OFF \
-D WITH_NGRAPH=OFF \
-D WITH_JASPER=OFF \
-D WITH_OPENJPEG=OFF \
-D WITH_WEBP=OFF \
-D WITH_OPENEXR=OFF \
-D WITH_TIFF=OFF \
-D WITH_OPENVX=OFF \
-D WITH_GDCM=OFF \
-D WITH_TBB=OFF \
-D WITH_OPENMP=ON \
-D WITH_HPX=OFF \
-D WITH_EIGEN=OFF \
-D WITH_V4L=ON \
-D WITH_LIBV4L=ON \
-D WITH_VTK=OFF \
-D WITH_QT=OFF \
-D WITH_ANDROID_MEDIANDK=OFF \
-D WITH_PROTOBUF=OFF \
-D OPENCV_DNN_OPENCL=OFF \
-D BUILD_opencv_python3=OFF \
-D BUILD_opencv_java=OFF \
-D BUILD_opencv_gapi=OFF \
-D BUILD_opencv_objc=OFF \
-D BUILD_opencv_js=OFF \
-D BUILD_opencv_ts=OFF \
-D BUILD_opencv_dnn=OFF \
-D BUILD_opencv_calib3d=OFF \
-D BUILD_opencv_objdetect=OFF \
-D BUILD_opencv_stitching=OFF \
-D BUILD_opencv_ml=OFF \
-D BUILD_opencv_world=OFF \
-D BUILD_EXAMPLES=OFF \
-D OPENCV_ENABLE_NONFREE=OFF \
-D OPENCV_GENERATE_PKGCONFIG=ON \
-D INSTALL_C_EXAMPLES=OFF \
-D INSTALL_PYTHON_EXAMPLES=OFF \
-D CMAKE_C_FLAGS="-march=armv8-a -mtune=cortex-a53 -O3 -pipe" \
-D CMAKE_CXX_FLAGS="-march=armv8-a -mtune=cortex-a53 -O3 -pipe" ../opencv
```

This configuration is optimized for the Raspberry Pi Zero 2 W platform. `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` should be adjusted based on the target architecture and optimization requirements.
