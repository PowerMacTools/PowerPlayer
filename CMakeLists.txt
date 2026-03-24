cmake_minimum_required(VERSION 3.6)

project(PowerPlayer LANGUAGES C CXX)

file(GLOB_RECURSE sources
    src/*.c src/*.h src/*.cpp src/*.hpp src/*.r
)
file(GLOB_RECURSE data resources/*)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME MATCHES Retro)
    add_application(${PROJECT_NAME} ${sources} ${data})
    add_compile_definitions(__RETRO68__)
else()
    add_executable(${PROJECT_NAME} ${sources} ${data})
endif()

target_link_libraries(${PROJECT_NAME} Mw)

message(CMAKE_C_COMPILER ": " ${CMAKE_C_COMPILER})
message(CMAKE_CXX_COMPILER ": " ${CMAKE_CXX_COMPILER})

if(PLATFORM MATCHES retro68)
    add_compile_definitions(FOR_68K)
endif()

if(PLATFORM MATCHES retroppc)
    add_compile_definitions(FOR_PPC)
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-ffunction-sections -mcpu=601 -Os -Wall -Wextra -Wno-unused-parameter")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
    target_link_libraries( ${PROJECT_NAME} OpenGL)
endif()

# TODO: vendor
target_link_libraries( ${PROJECT_NAME} png)

# ffmpeg/opengl
if(CMAKE_SYSTEM_NAME MATCHES Retro)
    include_directories(${CMAKE_SOURCE_DIR}/vendored/FFmpeg)
    target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/lib/lib/)
    include(ExternalProject)
    ExternalProject_Add(
        libff
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendored/FFmpeg
        CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/vendored/FFmpeg/configure --prefix=${CMAKE_SOURCE_DIR}/lib --arch=ppc32 --target-os=none --cross-prefix=powerpc-apple-macos- --enable-cross-compile --disable-pthreads --disable-altivec --enable-lasx
        PREFIX ${CMAKE_SOURCE_DIR}/vendored/FFmpeg
        BUILD_COMMAND make -j$(nproc)
        BUILD_IN_SOURCE 1
    )

    add_dependencies(${PROJECT_NAME} libff)

    target_link_libraries(${PROJECT_NAME} avcodec)
    target_link_libraries(${PROJECT_NAME} avdevice)
    target_link_libraries(${PROJECT_NAME} avfilter)
    target_link_libraries(${PROJECT_NAME} avformat)
    target_link_libraries(${PROJECT_NAME} avutil)
    target_link_libraries(${PROJECT_NAME} m)
    target_link_libraries(${PROJECT_NAME} avcodec)
    target_link_libraries(${PROJECT_NAME} swresample)
    target_link_libraries(${PROJECT_NAME} swscale)

    include_directories(${CMAKE_SOURCE_DIR}/vendored/include)
    link_directories(${CMAKE_SOURCE_DIR}/vendored/lib)

    target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/vendored/lib/libgl.a)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/vendored/lib/libglmem.a)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/vendored/lib/libglutil.a)
    target_link_libraries(${PROJECT_NAME} ContextualMenu)
    target_link_libraries(${PROJECT_NAME} DrawSprocketLib)

else() # Linux: just get it via the package manager.
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    include_directories( ${OPENGL_INCLUDE_DIRS}  ${GLUT_INCLUDE_DIRS} )

    message("GLUT_LIBRARY: ${GLUT_LIBRARY}")

    target_link_libraries(${PROJECT_NAME} OpenGL::GL GLUT::GLUT m)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavdevice
        libavfilter
        libavformat
        libavcodec
        libswresample
        libswscale
        libavutil
    )

    target_link_libraries(${PROJECT_NAME}
        PkgConfig::LIBAV
    )

endif()
include_directories( "${RETRO68_TOOLCHAIN}universal/CIncludes/")
message(STATUS "Making for ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,--mac-strip-macsbug")
endif()

# this copies all resource files in the build directory
# we need this, because we want to work with paths relative to the executable
file(COPY ${data} DESTINATION resources)
