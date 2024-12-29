cmake_minimum_required(VERSION 3.10)
project(Milky_Linux_OSD C)

# C language standard of 1999
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# find the PulseAudio package
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSEAUDIO REQUIRED libpulse)

# Find SDL2 package
find_package(SDL2 REQUIRED)

# Find GLFW
find_package(glfw3 REQUIRED)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find GLEW
find_package(GLEW REQUIRED)

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW) # Prefer GLVND
endif()


# Find OpenMP
find_package(OpenMP REQUIRED)
if(OpenMP_C_FOUND)
    message(STATUS "OpenMP found")
else()
    message(FATAL_ERROR "OpenMP not found")
endif()

# Add include directories
include_directories(
    ${PULSEAUDIO_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
)

# include all source files from ./src
file(GLOB SOURCES 
    "src/*.c"       # entrypoint, config, memory management etc.
    "src/audio/*.c" # waveform analyzing
    "src/audio/kiss_fft/*.c" # FFT analysis
    "src/video/*.c" # rendering the framebuffer
    "src/video/effects/*.c" # Video effects implementation (rendered on top)
)

# executable target
add_executable(milky_osd ${SOURCES})

# pulseaudio libs linked
target_link_libraries(
    milky_osd 
    ${PULSEAUDIO_LIBRARIES} 
    ${SDL2_LIBRARIES} 
    glfw
    GLEW::GLEW
    ${OPENGL_LIBRARIES}
    pthread 
    m
    OpenMP::OpenMP_C 
)

# compile flags
target_compile_options(milky_osd PRIVATE 
    # Apply only to GCC and Clang compilers
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:
        -O3
        -march=native
        -mavx -mavx2 -mfma # Enable AVX, AVX2, and FMA if supported
    >
    # Apply to Intel compilers
    $<$<C_COMPILER_ID:Intel>:
        -O3
        -xHost
        -axCORE-AVX2,FMA # Optimize for Intel CPUs with AVX2 and FMA
    >
-Wall -Wextra ${PULSEAUDIO_CFLAGS_OTHER})

#target_compile_definitions(milky_osd PRIVATE _OPENMP)
# Enable Link-Time Optimization (LTO) if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)
if(lto_supported)
    set_target_properties(milky_osd PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO/LTO not supported: ${error}")
endif()