
INCLUDE(${QT_USE_FILE})

QT4_WRAP_CPP(QTKPT_MOC_FILES 
    script.h
    sgraphics.h
    spoint.h
    spolyora_target.h
    stexture.h
    stime.h
    )

ADD_LIBRARY(script
    homography.h
    homography.cpp
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
    ${QTKPT_MOC_FILES}
)

INCLUDE_DIRECTORIES( ${OpenCV_INCLUDE_DIRS} )

