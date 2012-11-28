CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0)
PROJECT(polyora)
ENABLE_TESTING()

SET(CPACK_PACKAGE_VENDOR "Julien Pilet")
SET(CPACK_PACKAGE_VERSION_MAJOR 1)
SET(CPACK_PACKAGE_VERSION_MINOR 0)
SET(CPACK_PACKAGE_VERSION_PATCH 2)
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.txt")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Polyora is a multi-target tracking and retrieval library")
SET(CPACK_SOURCE_IGNORE_FILES "/\\\\.svn/" "/data/" "/tags" "/doc/" "/SiftGPU/" ".*\\\\.pro" "\\\\..*\\\\.swp"
		"/build/" "/\\\\.DS_Store")
SET(CPACK_SOURCE_GENERATOR ZIP)

INCLUDE(CPack)

SET(polyora_CMAKE_DIR "${polyora_SOURCE_DIR}/cmake")
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${polyora_CMAKE_DIR}")

set(OpenMP_LIB "")
FIND_PACKAGE(OpenMP)
IF (OPENMP_FOUND)
	OPTION(POLYORA_USE_OPENMP "Use OpenMP for multithreading" ON)	
	IF (POLYORA_USE_OPENMP)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_CXX_FLAGS}")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
                if (UNIX) 
                        # TODO: check if gomp is needed / exists.
                        SET(OpenMP_LIB gomp)
                endif (UNIX)
	ENDIF(POLYORA_USE_OPENMP)
ENDIF(OPENMP_FOUND)

OPTION(POLYORA_PROFILING "Self-profiling support" OFF)
OPTION(POLYORA_DISABLE_ASSERTIONS "Disable runtime integrity checks (assert). Faster, but make debugging more difficult." OFF)	
IF (POLYORA_DISABLE_ASSERTIONS)
	add_definitions(-DNDEBUG)
ENDIF (POLYORA_DISABLE_ASSERTIONS)

# OpenGL
FIND_PACKAGE(OpenGL)
IF(OPENGL_FOUND) 
	MESSAGE(STATUS "Looking for OpenGL - found")
ELSE(OPENGL_FOUND) 
	MESSAGE(STATUS "Looking for OpenGL - not found")
ENDIF(OPENGL_FOUND)

# Qt
SET(Qt4_FIND_REQUIRED OFF)
SET(QT_USE_QTOPENGL ON)
SET(QT_USE_QTSCRIPT ON)
FIND_PACKAGE(Qt4)
IF(QT4_FOUND) 
	MESSAGE(STATUS "Looking for Qt4 - found")
ELSE(QT4_FOUND) 
	MESSAGE(STATUS "Looking for Qt4 - not found")
ENDIF(QT4_FOUND)

IF(APPLE)
	ADD_DEFINITIONS(-DMACOS)
ENDIF(APPLE)

# OpenCV
SET(OpenCV_LIB_COMPONENTS opencv_features2d opencv_video opencv_highgui opencv_imgproc opencv_core )
FIND_PACKAGE(OpenCV REQUIRED)
IF(OpenCV_FOUND) 
	MESSAGE(STATUS "Looking for OpenCV - found")
ELSE(OpenCV_FOUND) 
	MESSAGE(FATAL_ERROR "Looking for OpenCV - not found")
ENDIF(OpenCV_FOUND)
SET(OpenCV_LIBS ${OpenCV_LIBS})
SET(OpenCV_LIBRARIES ${OpenCV_LIBS})

FIND_PACKAGE(GLEW)

INCLUDE_DIRECTORIES(.)

SET(CPACK_COMPONENTS_ALL polyora_lib_headers tools videosource_lib_headers)
SET(CPACK_COMPONENT_polyora_LIB_HEADERS_DISPLAY_NAME "Polyora library")
SET(CPACK_COMPONENT_VIDEOSOURCE_LIB_HEADERS_DISPLAY_NAME "Videosource library (optional)")
SET(CPACK_COMPONENT_TOOLS_DISPLAY_NAME "Tools")

ADD_SUBDIRECTORY(videosource)
ADD_SUBDIRECTORY(polyora)
ADD_SUBDIRECTORY(buildtree)
ADD_SUBDIRECTORY(cluster_ids)
ADD_SUBDIRECTORY(crawler)
ADD_SUBDIRECTORY(examples)
IF (QT4_FOUND)
	ADD_SUBDIRECTORY(qtpolyora)
	ADD_SUBDIRECTORY(vobj_tracker)
	ADD_SUBDIRECTORY(track3d)
        ADD_SUBDIRECTORY(script)
ENDIF (QT4_FOUND)

