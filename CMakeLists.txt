cmake_minimum_required(VERSION 3.6)

project(ppcplayer LANGUAGES C CXX)


file(GLOB_RECURSE sources
    src/*.c src/*.h src/*.cpp src/*.hpp src/*.r
)
file(GLOB_RECURSE data resources/*)
# you can use set(sources src/main.cpp) etc if you don't want to
# use globbing to find files automatically

###############################################################################
## target definitions #########################################################
###############################################################################

# add the data to the target, so it becomes visible in some IDE

if(CMAKE_SYSTEM_NAME MATCHES Retro)
add_application(ppcplayer ${sources} ${data})
add_compile_definitions(__RETRO68__)
else()
add_executable(ppcplayer ${sources} ${data})
endif()

set_property(TARGET ppcplayer PROPERTY CXX_STANDARD 20)

if(PLATFORM MATCHES retro68)
add_compile_definitions(FOR_68K)
endif()

if(PLATFORM MATCHES retroppc)
add_compile_definitions(FOR_PPC)
set_target_properties(ppcplayer PROPERTIES COMPILE_FLAGS "-ffunction-sections -mcpu=601 -O3 -Wall -Wextra -Wno-unused-parameter")
set_target_properties(ppcplayer PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
target_link_libraries( ppcplayer PowerMgrLib)
target_link_libraries( ppcplayer NameRegistryLib)
target_link_libraries( ppcplayer OpenGL)
endif()

message(CMAKE_C_COMPILER ": " ${CMAKE_C_COMPILER})
message(CMAKE_CXX_COMPILER ": " ${CMAKE_CXX_COMPILER})





# Retro68: compile ffmpeg and link against the OpenGL stub.
if(CMAKE_SYSTEM_NAME MATCHES Retro)
    include_directories(${CMAKE_SOURCE_DIR}/vendored/FFmpeg)
    target_link_directories(ppcplayer PUBLIC ${CMAKE_SOURCE_DIR}/lib/lib/)
    include(ExternalProject)
    ExternalProject_Add(
        libff
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendored/FFmpeg
        CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/vendored/FFmpeg/configure --prefix=${CMAKE_SOURCE_DIR}/lib --arch=ppc32 --target-os=none --cross-prefix=powerpc-apple-macos- --enable-cross-compile --disable-pthreads --disable-altivec --enable-lasx
        PREFIX ${CMAKE_SOURCE_DIR}/vendored/FFmpeg
        BUILD_COMMAND make -j$(nproc)
        BUILD_IN_SOURCE 1
    )

    add_dependencies(ppcplayer libff)

    target_link_libraries(ppcplayer avcodec)
    target_link_libraries(ppcplayer avdevice)
    target_link_libraries(ppcplayer avfilter)
    target_link_libraries(ppcplayer avformat)
    target_link_libraries(ppcplayer avutil)
    target_link_libraries(ppcplayer m)
    target_link_libraries(ppcplayer avcodec)
    target_link_libraries(ppcplayer swresample)
    target_link_libraries(ppcplayer swscale)

    include_directories(${CMAKE_SOURCE_DIR}/vendored/include)
    link_directories(${CMAKE_SOURCE_DIR}/vendored/lib)

    target_link_libraries(ppcplayer ${CMAKE_SOURCE_DIR}/vendored/lib/libgl.a)
    target_link_libraries(ppcplayer ${CMAKE_SOURCE_DIR}/vendored/lib/libglmem.a)
    target_link_libraries(ppcplayer ${CMAKE_SOURCE_DIR}/vendored/lib/libglutil.a)
    target_link_libraries(ppcplayer ContextualMenu)
    target_link_libraries(ppcplayer DrawSprocketLib)

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
set_target_properties(ppcplayer PROPERTIES LINK_FLAGS "-Wl,--mac-strip-macsbug")
endif()

# this copies all resource files in the build directory
# we need this, because we want to work with paths relative to the executable
file(COPY ${data} DESTINATION resources)
