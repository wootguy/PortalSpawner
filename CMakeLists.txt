cmake_minimum_required(VERSION 3.6)

project(PortalSpawner)

set(SOURCE_FILES
	src/main.h src/main.cpp
	src/Portal.h src/Portal.cpp
)

add_library(${PROJECT_NAME} SHARED)
add_subdirectory(mmlib)
mmlib_default_setup()
