# Library target
add_library(utils
    src/utils.cpp
)

# Specify the include directories for the library
target_include_directories(utils
    PUBLIC
        include
)

# Specify the dependencies for the library
target_link_libraries(motionLib
   PRIVATE
       compilerFlags
)
