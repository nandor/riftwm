CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(riftwm)

SET(SOURCES riftwm.c
            kinect.c
            renderer.c)

SET(HEADERS riftwm.h
            renderer.h
            kinect.h)

SET(LIBS GLEW GL GLU X11 IL ILU Xcomposite Xdamage m openhmd)

ADD_DEFINITIONS(-D_USE_MATH_DEFINES)

ADD_EXECUTABLE(riftwm ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(riftwm ${LIBS})