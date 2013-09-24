
SET(EXECUTABLE vobj_tracker)

ADD_EXECUTABLE(${EXECUTABLE} glbox.cpp glbox.h ipltexture.cpp
        ipltexture.h main.cpp vobj_tracker_view.cpp vobj_tracker_view.h )

ADD_DEPENDENCIES(${EXECUTABLE} polyora script)

IF (WIN32)
	ADD_DEPENDENCIES(${EXECUTABLE} videosource)
ENDIF(WIN32)
	
INCLUDE_DIRECTORIES( ${OpenCV_INCLUDE_DIRS} ${VIDEOSOURCE_INCLUDE_DIR})

TARGET_LINK_LIBRARIES(${EXECUTABLE} script polyora 
	${VIDEOSOURCE_LIBRARIES}
	${OpenCV_LIBS}
	${QT_LIBRARIES}
	${OPENGL_gl_LIBRARY}
	)

INSTALL(TARGETS vobj_tracker RUNTIME DESTINATION bin COMPONENT tools)
