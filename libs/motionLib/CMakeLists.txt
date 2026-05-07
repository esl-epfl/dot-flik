# Library target
add_library(motionLib
    src/motionLib.cpp
)

# Specify the include directories for the library
target_include_directories(motionLib
    PUBLIC
        include
)

# Specify the dependencies for the library
target_link_libraries(motionLib
    PUBLIC
        ${OpenCV_LIBS}
    PRIVATE
        compilerFlags
)