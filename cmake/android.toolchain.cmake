# CMAKE_TOOLCHAIN_FILE=android.toolchain.cmake cmake ..
# This is a placeholder for Android NDK toolchain configuration

set(ANDROID_PLATFORM android-21)
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)
set(CMAKE_ANDROID_STL_TYPE c++_static)

# Add Android specific definitions
add_definitions(-DANDROID)

# Link log library
# target_link_libraries(${PROJECT_NAME} log android)
