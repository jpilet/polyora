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
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <highgui.h>
#include <polyora/kpttracker.h>

using namespace std;

int main(int argc, char **argv) 
{
	int max_level = 8;
	int min_elem = 1000;
	const char *descr_fn = "descriptors.dat";
	const char *tree_fn = 0;
	const char *cvt_tree_fn = 0;
	const char *db_fn = "visual.db";
	int stop=0;
		
	for (int i=1; i<argc; i++) {
		if (i<argc-1) {
			if (strcmp(argv[i],"-d")==0) {
				descr_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-t")==0) {
				tree_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-C")==0) {
				cvt_tree_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-v")==0) {
				db_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-r")==0) {
				max_level = atoi(argv[++i]);
				continue;
			} else if (strcmp(argv[i],"-e")==0) {
				min_elem = atoi(argv[++i]);
				continue;
			} else if (strcmp(argv[i],"-s")==0) {
				stop = atoi(argv[++i]);
				continue;
			} 
		}
		cout << "Available options:\n"
			" -d <descriptor file>\n"
		        " -t <tree file>\n"
		        " -v <database filename>\n"
		        " -r <max recursion>\n"
		        " -e <min number of elements>\n"
		        " -s <max elements to process>\n"
			" -C <tree file> convert the tree file to sqlite3 database\n"
		       ;

		return(-1);
	}

	if (cvt_tree_fn && tree_fn) {
		cerr << "-t and -C options are incompatible.\n";
		return -1;
	}
	if (cvt_tree_fn && !db_fn) {
		cerr << "when using -C, please specify the database target with -v <db>.\n";
		return -1;
	}
	kmean_tree::node_t *root;
	if (!cvt_tree_fn)
		root = kmean_tree::build_from_data(descr_fn, max_level, min_elem, stop);
	else
		root = kmean_tree::load(cvt_tree_fn);

	if (!root) {
		std::cerr << descr_fn << ": unable to build tree from descriptor file\n";
		return -1;
	}
	std::cout << "Done. Saving result..." << std::endl;
	if (tree_fn)
		root->save(tree_fn);
	else {
		if (!root->save_to_database(db_fn)) {
			cerr << db_fn << ": error.\n";
			return -1;
		}
	}

	return 0;
}

template < class T >
string ToString(const T &arg)
{
	ostringstream	out;

	out << arg;

	return(out.str());
}
