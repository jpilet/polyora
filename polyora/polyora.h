/*  This file is part of Polyora, a multi-target tracking library.
    Copyright (C) 2010 Julien Pilet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program. If not, see <http://www.gnu.org/licenses/>.

    To contact the author of this program, please send an e-mail to:
    julien.pilet(at)calodox.org
*/
/*!

\mainpage

\section intro Introduction

Polyora is a library to retrieve images and videos from tracked feature points.

\section license License

You are allowed to use polyora under the terms of the GNU General Public
License, version 2 or above.
In short, it means that any distributed project linking with polyora or that
includes any portion of its source code should be released under the same
license, with the source code. The details of the license can be found there:
http://www.gnu.org/licenses/old-licenses/gpl-2.0.html

If the terms of the GPL license do not meet your requirements, you are welcome
to contact Julien Pilet (julien.pilet(at)calodox.org) to find a solution.

\section Download

- Sources: http://www.hvrl.ics.keio.ac.jp/~julien/polyora/polyora-1.0.2-Source.zip
- Windows binaries: http://www.hvrl.ics.keio.ac.jp/~julien/polyora/polyora-1.0.1-win32.exe
- Sample Data: http://www.hvrl.ics.keio.ac.jp/~julien/polyora/polyora-data.zip

\section Installation

Polyora depends on a number of software and libraries to compile properly:

- cmake (http://www.cmake.org/)
- opencv (http://opencv.willowgarage.com/)

Additionally, you may want to use QT (http://qt.nokia.com/) for the GUI.

If you prefer to use SIFT rather than the provided feature detector/descriptor,
polyora can use SiftGPU (see \ref siftgpu).
  
Polyora has been compiled successfully on Windows, Linux, and Mac OS.

Quick installation instructions:
-# run cmake or cmake-gui on CMakeLists.txt
-# type 'make' or use the appropriate tool to build the project created by cmake.

See also \ref win32compile .

\section Usage

Using polyora is not difficult but not straightforward. 
Usage is explained there: \ref howto.

Examples:
- \ref simpletrack.cpp : A basic example showing how to track features.
- \ref vobj_tracker_view : The full application that detect and tracks objects.

\section internals Internals

Polyora is organized in several levels:
-# \ref TracksGroup - Structures for sparse storage of tracked keypoints.
-# \ref KptTrackingGroup - Algorithms to fill the structured already described.
-# \ref RetrievalGroup - Indexing and retrieval without any geometric knowledge, based on quantized point track description.
-# \ref ObjectTrackingGroup - RANSAC and other algorithms for multiple planar object tracking.


\section author Author
Most of polyora was written by Julien Pilet, at Keio University, Japan, from Dec. 2008 to March 2010.

\section ack Acknowledgements

I sincerely thank Hideo Saito for supporting me on this project.
I warmly thank Vincent Lepetit and Pascal Fua who taught me so
much during my PhD studies. Vincent, even when you're on the other side of the
world, you keep inspiring me !


\page howto Basic usage instructions

\section usage_intro Basics

Polyora comes with a couple of executables. The most interesting one is: vobj_tracker.
It can retrieve, track, and augment planar objects (see \ref tracking).
QtPolyora is similar, but lower level. Its goal is to provide better
visualization of the retrieval process. It can also be used to collect training
data.

Both executables rely on trained vector quantization prototypes. Such data can
be trained with the procedure described in \ref training. Alternatively,
pre-trained data can be downloaded from Polyora's web site.

- Download: http://www.hvrl.ics.keio.ac.jp/~julien/polyora/polyora-data.zip
- unzip its content in a directory (On Windows, that could be: C:\\Program Files\\Polyora\\bin)
- execute vobj_tracker from this directory

Please note that the pre-trained data might not work well with your
configuration. Training your own data is more reliable.

\section training Training Stage
<ol>
<li> Create a directory for your data ("data" for example)
<li> Collect point descriptors
	\verbatim ../qtpolyora/qtpolyora -rp -d descriptors.dat -v visual.db \endverbatim
<li> Build a K-mean tree: \verbatim ../buildtree/buildtree -d descriptors.dat -r 8 -e 64 -v visual.db \endverbatim
<li> Collect tracks of feature points: \verbatim ../qtpolyora/qtpolyora -rt -d tracks.dat -v visual.db \endverbatim
<li> cluster tracks: \verbatim ../cluster_ids/cluster_ids -d tracks.dat -T .1 -v visual.db \endverbatim
</ol>

The resulting visual.db should contain the quantization tree for descriptors, and the prototypes for quantizing tracks.
Visual objects can also be stored in the same file.

If you have large files (>2 GB), compiling and running these tools in a 64 bits
environment might be necessary. Please note also that cluster_ids can require a
large amount of memory.

\section tracking Tracking planar objects

The easiest way to start tracking objects with Polyora is to use the included
Visual Object Track (vobj_tracker). The procedure is the following:

<ol>
<li> Either run the training stage described previously or download "visual.db" as described above.
<li> Connect a recognized camera.
<li> Run the visual object tracker:
	\verbatim vobj_tracker -v visual.db \endverbatim
<li> Take a frontal view of a planar, textured object and press 'H'.
<li> Tracking starts. Repeat (4) as many time as required. Everything is automatically saved in visual.db.
</ol>

\page siftgpu Using Polyora with SiftGPU

Polyora is more accurate and more reliable, but slower with SiftGPU. SiftGPU is
not included in polyora because its license is incompatible with GPL.

<ol>
<li> Download SiftGPU from  http://www.cs.unc.edu/~ccwu/siftgpu/
<li> Decompress and compile the archive in the polyora source directory
<li> Edit the file polyora/kpttracker.h, and replace:
\verbatim 
//#define WITH_SIFTGPU
#define WITH_YAPE
\endverbatim
by
\verbatim
#define WITH_SIFTGPU
//#define WITH_YAPE
\endverbatim
<li> Run cmake to configure polyora. In the output, make sure cmake says: SiftGPU is ON.
<li> Compile polyora
<li> Re-create training data (see \ref howto)
</ol>

\page win32compile Compiling Polyora with Microsoft Visual Studio

<ol>
<li> Download OpenCV from http://opencv.willowgarage.com/
<li> If CMake is not installed, install it from http://www.cmake.org/
<li> The OpenCV distribution does not include MSVC binaries. You will need to compile them:
<ol>
<li> Create a directory for the build, for example: C:\\opencv20\\build
<li> Start cmake-gui, select the opencv source directory and the build directory
<li> Click "Configure" twice, and "Generate"
<li> cmake should create a visual studio solution called OpenCV.sln. Open it.
<li> Select build/build solution (F7)
<li> Right-click on the INSTALL project, and click "build". That should be it!
</ol>
<li> Everything should be ready to compile Polyora. Run cmake-gui and select the Polyora source directory.
<li> Click on "add entry"
<li> In Name, enter: OpenCV_ROOT_DIR
<li> In Type, select "PATH"
<li> In Value, enter the OpenCV build directory (for example: c:\\opencv20\\build)
<li> Click OK, Configure, and Generate.
You should obtain a Visual Studio project file called polyora.sln.
<li> Open polyora.sln and select "Build Solution" from the "Build" menu.
</ol>


*/

/*!
\example simpletrack.cpp This example demonstrates how to track points in a
sequence of images using polyora.
*/

#include "tracks.h"
#include "kpttracker.h"
#include "visual_database.h"
#include "vobj_tracker.h"
