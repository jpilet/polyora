
INCLUDE_DIRECTORIES( ${OpenCV_INCLUDE_DIRS} )
IF (SIFTGPU_FOUND)
	INCLUDE_DIRECTORIES( ${SIFTGPU_INCLUDE_DIRS} )
ENDIF (SIFTGPU_FOUND)

ADD_LIBRARY(polyora 
	bucket2d.h
	distortion.cpp distortion.h
	idcluster.cpp idcluster.h
	keypoint.h
	kmeantree.cpp kmeantree.h
	kpttracker.cpp kpttracker.h
	mlist.h
	mylkpyramid.cpp mylkpyramid.h
	patchtagger.cpp patchtagger.h
	preallocated.h
	pyrimage.cpp pyrimage.h
	sqlite3.c sqlite3.h
	timer.cpp timer.h
	tracks.cpp tracks.h
	vecmap.h
	visual_database.cpp visual_database.h
	vobj_tracker.cpp vobj_tracker.h
	yape.cpp yape.h
	adapt_thresh.cpp adapt_thresh.h
	mserdetector.cpp mserdetector.h
	homography4.h homography4.cpp fvec4.h
	polyora.h
	# FAST is not included in the polyora distribution.
	# fast.c fast.h fast_10.c fast_11.c fast_12.c fast_9.c nonmax.c
	)

TARGET_LINK_LIBRARIES(polyora ${OpenCV_LIBS})

IF (POLYORA_PROFILING)
	ADD_DEFINITIONS(-DENABLE_PROFILE)
ENDIF(POLYORA_PROFILING)

SET_TARGET_PROPERTIES(polyora PROPERTIES PUBLIC_HEADER 
"polyora.h;tracks.h;vobj_tracker.h;visual_database.h;kpttracker.h;kmeantree.h;idcluster.h;vecmap.h;bucket2d.h;patchtagger.h;mlist.h;yape.h;keypoint.h;pyrimage.h;sqlite3.h;timer.h")

SET_TARGET_PROPERTIES(polyora PROPERTIES DEBUG_POSTFIX "_d")

INSTALL(TARGETS polyora 
		ARCHIVE DESTINATION "lib" 
		PUBLIC_HEADER DESTINATION "include/polyora" 
		COMPONENT polyora_lib_headers)
