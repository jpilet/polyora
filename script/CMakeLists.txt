
ADD_LIBRARY(script
    homography.h
    homography.cpp
    sattachedpoint.cpp
    sattachedpoint.h
    script.cpp
    script.h
    sgraphics.cpp
    sgraphics.h
    spoint.cpp
    spoint.h
    spolyora_target.cpp
    spolyora_target.h
    stexture.cpp
    stexture.h
    stime.cpp
    stime.h
)

TARGET_LINK_LIBRARIES(script ${QT_LIBRARIES})

INCLUDE_DIRECTORIES( ${OpenCV_INCLUDE_DIRS} )

