# CameraStreamer.cpp 
add_executable(CameraStreamer CameraStreamer.cpp)
target_link_libraries(CameraStreamer
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

add_executable(testFormat testFormat.cpp)
target_link_libraries(testFormat
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

add_executable(cameraWifiStream cameraWifiStream.cpp)
target_link_libraries(cameraWifiStream
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)