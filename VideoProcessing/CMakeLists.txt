# VideoProcessing.cpp 
add_executable(VideoProcessing VideoProcessing.cpp)
target_link_libraries(VideoProcessing
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

# HyperpTuning.cpp
add_executable(HyperpTuning HyperpTuning.cpp)
target_link_libraries(HyperpTuning
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

# VideoFiltering.cpp
add_executable(VideoFiltering VideoFiltering.cpp)
target_link_libraries(VideoFiltering
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

# stressTest.cpp
add_executable(stressTest stressTest.cpp)
target_link_libraries(stressTest
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)
