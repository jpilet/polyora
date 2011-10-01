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
#include <videosource.h>
#include <iostream>
#include <iniparser.h>
#include <qapplication.h>
#include <fstream>
#include "glbox.h"
#include "vsview.h"

using namespace std;

int main(int argc, char **argv) {
	QApplication a( argc, argv );

        // create a .ini file parser
        IniParser parser(0);

        // Create the video source:
	// 1) construct .ini parser
	// 2) parse .ini file
	// 3) try to construct a valid video source
        VideoSourceFactory factory;
	factory.registerParameters(&parser);

	const char *vs_ini = "videosource.ini";

	for (int i=1; i<argc-1; ++i) {
		if (strcmp(argv[i],"-vs")==0) {
			vs_ini = argv[++i];
		}
	}

	if (!parser.parse(vs_ini)) {
		cerr << "warning: unable to read " << vs_ini << ", generating an example one.\n";
		parser.dumpExampleFile(const_cast<char *>(vs_ini));
	}

        // try to get a video source
        VideoSource *vs = factory.construct();

        if (vs==0) {
                cerr << "Unable to open video source!\n";
                return 2;
        }

	VSView *glbox = new VSView(0, "GLBox", vs);

	for (int i=1; i<argc; i++) {
		if (i<argc-1) {
			if (strcmp(argv[i],"-t")==0) {
				glbox->tree_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-d")==0) {
				glbox->descriptors_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-v")==0) {
				glbox->visual_db_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-s")==0) {
				if (!glbox->load_script(argv[++i]))
					cerr << argv[i] << ": failed to load script\n";
				continue;
			} else if (strcmp(argv[i],"-vs")==0) {
				++i;
				continue;
			} else if (strcmp(argv[i],"-c")==0) {
				glbox->clusters_fn = argv[++i];
				continue;
			}
		} 
		if (strcmp("-rt",argv[i])==0) {
			glbox->record = true;
			glbox->record_pts = false;
		} else if (strcmp("-B",argv[i])==0) {
			glbox->query_flags |= id_cluster_collection::QUERY_BIN_FREQ;
		} else if (strcmp("-M",argv[i])==0) {
			glbox->query_flags |= id_cluster_collection::QUERY_MIN_FREQ;
		} else if (strcmp("-p", argv[i])==0) {
			glbox->use_pipeline = true;
		} else if (strcmp("-rp",argv[i])==0) {
			glbox->record = false;
			glbox->record_pts = true;
		} else {
			cout << "Unknown command line parameter: " << argv[i] << endl;
			cout << "Available options are:\n"
				" -t <tree file>\n"
				" -d <output descriptor file>\n"
				" -v <visual database file>\n"
				" -c <clusters file>\n"
				" -rt : record tracks\n"
				" -rp : recort point descriptors\n"
				" -s <script>\n"
				" -B : binary tf-idf\n"
				" -M : min tf-idf\n";
		}
	}

	//glbox->setWindowTitle("VideoSource viewer - julien@hvrl.ics.keio.ac.jp");
	glbox->show();
	
	a.connect( &a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()) );
	int r= a.exec();
	cout << "bye bye\n";
	delete glbox;
	return r;
}

