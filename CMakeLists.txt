cmake_minimum_required(VERSION 3.10)
project(SpotifyFS)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

# macFUSE setup
set(FUSE_INCLUDE_DIR "/usr/local/include/fuse")
set(FUSE_LIBRARIES "/usr/local/lib/libfuse.dylib")

# JsonCpp setup (for macOS/Homebrew)
set(JSONCPP_INCLUDE_DIR "/opt/homebrew/include")
set(JSONCPP_LIBRARIES "/opt/homebrew/lib/libjsoncpp.dylib")

# Check if macFUSE is installed
if(NOT EXISTS ${FUSE_INCLUDE_DIR}/fuse.h)
    message(FATAL_ERROR "macFUSE not found. Please install it using 'brew install macfuse' and try again.")
endif()

# Include directories
include_directories(
    ${FUSE_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIR}
    include
    ${CURL_INCLUDE_DIRS}
)

# Automatically find all source and header files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# Add executable target
add_executable(SpotifyFS ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(SpotifyFS
    ${FUSE_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    pthread
    ${CURL_LIBRARIES}
)

# Add macOS specific compile definitions
target_compile_definitions(SpotifyFS PRIVATE
    _FILE_OFFSET_BITS=64
    FUSE_USE_VERSION=32
)

# Add test directory
add_subdirectory(tests)