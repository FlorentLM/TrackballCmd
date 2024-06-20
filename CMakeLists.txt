#
# Created by Florent Le Moel on 02/05/2019.
#

cmake_minimum_required(VERSION 3.15)
project( TrackballControl )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Have CMake find our pthreads library within our toolchain (required for cross-compiling)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")

add_executable( TrackballControl
        fx2flash.cpp
        Trackball.cpp Trackball.h
        Visualizers.h Visualizers.cpp
        commandline.cpp)

# CyUSB
link_directories( "${CYUSB_ROOT}" "cyusb" )
#link_directories( "Cypress/" "cyusb" )
#target_link_libraries( TrackballControl cyusb )

# OpenCV
find_package( OpenCV REQUIRED )

include_directories( ${OpenCV_INCLUDE_DIRS} )
target_link_libraries( TrackballControl ${OpenCV_LIBS} )


# add the Threads library we found with the command above
target_link_libraries(TrackballControl ${CMAKE_THREAD_LIBS_INIT})