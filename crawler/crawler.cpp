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
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <highgui.h>
#include "pic_randomizer.h"


using namespace std;

void usage(const char *name)
{
	printf( "Usage: %s <options> [-] [<image1> <image2> ... ]\n"
		" -n <number of views>\n"
		" -r <min view rate>\n"
		" -C <output file>     file to write quantized descriptors to.\n"
		" -D <file>            write descriptors without quantization to <file>\n"
		" -v <database>        database to load the tree from.\n"
		" -t <tree file>       load tree from file\n"
		"If %s is invoked with the argument '-', file names are read from stdin, one per line.\n"
		, name, name);
}

int main(int argc, char **argv)
{
	pic_randomizer generator;

	vector<string> files;

	bool read_stdin=false;

	for (int i=1; i<argc; i++) {
		if (i<argc-1) {
			if (strcmp(argv[i], "-n")==0) {
				generator.nb_views = atoi(argv[++i]);
				continue;
			} else if (strcmp(argv[i], "-D")==0) {
				generator.output_fn = argv[++i];
				generator.save_descriptors_only=true;
				continue;
			} else if (strcmp(argv[i], "-C")==0) {
				generator.output_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i], "-v")==0) {
				generator.visualdb_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i], "-t")==0) {
				generator.tree_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i], "-r")==0) {
				generator.min_view_rate = atof(argv[++i]);
				continue;
			}
		}

		if (strcmp(argv[i], "-")==0) {
			read_stdin = true;
		} else if (argv[i][0] == '-') {
			usage(argv[0]);
			return -1;
		} else {
			files.push_back(string(argv[i]));
		}
	}

	if (!generator.output_fn) {
		printf("Please specify either -C <output file> or -D <output file>.\n");
		usage(argv[0]);
		return -1;
	}

	while(read_stdin && cin.good()) {
		string s;
		getline(cin, s);
		if (s.length()==0) break;
		files.push_back(s);
	}


	int n = files.size();

	pic_randomizer gen(generator);
	for (int i=0; i<n; i++)
	{
		
		if (gen.load_image(files[i].c_str())) {
			cout << files[i] << endl;
			gen.run();
		}
	}
}

