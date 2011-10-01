# - Test for DirectShow on Windows.
# Once loaded this will define
#   DIRECTSHOW_FOUND        - system has DirectShow
#   DIRECTSHOW_INCLUDE_DIR  - include directory for DirectShow
#   DIRECTSHOW_LIBRARIES    - libraries you need to link to


# DirectShow is only available on Windows platforms
IF(WIN32)
  # Find DirectX Include Directory
  FIND_PATH(DIRECTX_INCLUDE_DIR ddraw.h
    "C:/Program Files/Microsoft Visual Studio .NET 2003/Vc7/PlatformSDK/Include"
    "C:/Program Files/Microsoft DirectX SDK (February 2006)/Include"
    "C:/Program Files/Microsoft DirectX 9.0 SDK (June 2005)/Include"
    "C:/DXSDK/Include"
    DOC "What is the path where the file ddraw.h can be found"
  )

  # if DirectX found, then find DirectShow include directory
  IF(DIRECTX_INCLUDE_DIR)
    FIND_PATH(DIRECTSHOW_INCLUDE_DIR dshow.h
      "C:/Program Files/Microsoft Visual Studio .NET 2003/Vc7/PlatformSDK/Include"
      "C:/Program Files/Microsoft Platform SDK/Include"
      "C:/DXSDK/Include"
      DOC "What is the path where the file dshow.h can be found"
    )

    # if DirectShow include dir found, then find DirectShow libraries
    IF(DIRECTSHOW_INCLUDE_DIR)
      FIND_LIBRARY(DIRECTSHOW_strmiids_LIBRARY strmiids
        "C:/Program Files/Microsoft Visual Studio .NET 2003/Vc7/PlatformSDK/Lib"
        "C:/Program Files/Microsoft Platform SDK/Lib"
        "C:/DXSDK/Include/Lib"
        DOC "Where can the DirectShow strmiids library be found"
      )
      FIND_LIBRARY(DIRECTSHOW_quartz_LIBRARY quartz
        "C:/Program Files/Microsoft Visual Studio .NET 2003/Vc7/PlatformSDK/Lib"
        "C:/Program Files/Microsoft Platform SDK/Lib"
        "C:/DXSDK/Include/Lib"
        DOC "Where can the DirectShow quartz library be found"
      )

      # if DirectShow libraries found, then we're ok
      IF(DIRECTSHOW_strmiids_LIBRARY)
      IF(DIRECTSHOW_quartz_LIBRARY)
        # everything found
        SET(DIRECTSHOW_FOUND "YES")
      ENDIF(DIRECTSHOW_quartz_LIBRARY)
      ENDIF(DIRECTSHOW_strmiids_LIBRARY)
    ENDIF(DIRECTSHOW_INCLUDE_DIR)
  ENDIF(DIRECTX_INCLUDE_DIR)

  SET(DIRECTSHOW_LIBRARIES
    ${DIRECTSHOW_strmiids_LIBRARY}
    ${DIRECTSHOW_quartz_LIBRARY}
    CACHE STRING "DirectShow Libraries"
  )

  SET(DIRECTSHOW_INCLUDE_DIR
    ${DIRECTSHOW_INCLUDE_DIR}
    ${DIRECTX_INCLUDE_DIR}
    
    CACHE STRING "Include directories for DirectShow"
  )

  MARK_AS_ADVANCED( DIRECTSHOW_INCLUDE_DIR DIRECTSHOW_LIBRARIES DIRECTSHOW_strmiids_LIBRARY DIRECTSHOW_quartz_LIBRARY)
ENDIF(WIN32)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
	DIRECTSHOW DEFAULT_MSG 
	DIRECTSHOW_LIBRARIES DIRECTSHOW_INCLUDE_DIR )

