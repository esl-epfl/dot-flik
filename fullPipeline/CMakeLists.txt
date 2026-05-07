# fullPipeline.cpp 
add_executable(fullPipeline fullPipeline.cpp)
target_link_libraries(fullPipeline
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

# fullPipeline_profile.cpp 
add_executable(fullPipeline_profile fullPipeline_profile.cpp)
target_link_libraries(fullPipeline_profile
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

# fullPipeline_threaded.cpp 
add_executable(fullPipeline_threaded fullPipeline_threaded.cpp)
target_link_libraries(fullPipeline_threaded
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

# fullPipeline_threaded_profile.cpp 
add_executable(fullPipeline_threaded_profile fullPipeline_threaded_profile.cpp)
target_link_libraries(fullPipeline_threaded_profile
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)

# fullPipeline_externalVideos.cpp 
add_executable(fullPipeline_externalVideos fullPipeline_externalVideos.cpp)
target_link_libraries(fullPipeline_externalVideos
    PRIVATE
        compilerFlags
        motionLib
        utils
        ${OpenCV_LIBS}
)