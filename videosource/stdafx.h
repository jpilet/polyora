// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#define _USE_MATH_DEFINES 
#define _WIN32_WINNT 0x400
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <iostream>
#include <vector>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>

void debugPrint(const char* szMessage, ...);
void errorPrint(const char* szMessage, ...);

