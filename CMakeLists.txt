CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(riftwm)

SET(SOURCES riftwm.c
            kinect.c
            renderer.c)

SET(HEADERS riftwm.h
            renderer.h
            kinect.h
            shader/warp.fs.glsl.h
            shader/warp.vs.glsl.h)

SET(LIBS GLEW GL GLU X11 Xcomposite Xdamage m openhmd)

FILE(GLOB_RECURSE SHADERS "shader/*.glsl")
ADD_CUSTOM_COMMAND(
  OUTPUT "shader/warp.fs.glsl.h"
  COMMAND xxd -i "../shader/warp.fs.glsl" "../shader/warp.fs.glsl.h"
)
ADD_CUSTOM_COMMAND(
  OUTPUT "shader/warp.vs.glsl.h"
  COMMAND xxd -i "../shader/warp.vs.glsl" "../shader/warp.vs.glsl.h"
)
ADD_DEFINITIONS(-D_USE_MATH_DEFINES)

ADD_EXECUTABLE(riftwm ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(riftwm ${LIBS})