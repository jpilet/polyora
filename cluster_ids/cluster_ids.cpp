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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <polyora/polyora.h>
using namespace std;

bool load_patch_track(id_cluster_collection *clusters, const char *descr_fn, kmean_tree::node_t *root)
{
	FILE *descr_f = fopen(descr_fn, "rb");
	if (!descr_f) {
		perror(descr_fn);
		return false;
	}

	id_cluster *c = new id_cluster();

	kmean_tree::descr_file_packet packet;
	while(fread(&packet, sizeof(packet), 1, descr_f) == 1) {
		unsigned id = root->get_id(&packet.d);
		if (packet.ptr <0 && c->total>0) {
			clusters->add_cluster(c);
			c = new id_cluster();
		}
		c->add(id, 1);
	}
	fclose(descr_f);
	return true;
}

int main(int argc, char **argv) 
{
	float threshold = .7;
	const char *descr_fn = "descriptors.dat";
	const char *tree_fn = 0;
	const char *clusters_fn = 0;
	const char *db_fn = "visual.db";
		
	bool cluster_format=false;

	for (int i=1; i<argc; i++) {
		if (i<argc-1) {
			if (strcmp(argv[i],"-d")==0) {
				descr_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-C")==0) {
				++i;
				continue;
			} else if (strcmp(argv[i],"-v")==0) {
				db_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-t")==0) {
				tree_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-T")==0) {
				threshold = atof(argv[++i]);
				continue;
			} else if (strcmp(argv[i],"-o")==0) {
				clusters_fn = argv[++i];
				continue;
			} else if (strcmp(argv[i],"-c")==0) {
				cluster_format=true;
				descr_fn = argv[++i];
				continue;
			}
		}
		cout << "Available options:\n"
			" -v <visual db> (database to write the result to)\n"
			" -C <visual db> (read clusters from this database)\n"
			" -c <cluster file>\n"
			" -d <descriptor file>\n"
			" -t <tree file>\n"
		       	" -T <threshold> (use -1 for convertion only)\n"
			" -o <output file>\n";
		return(-1);
	}

	sqlite3 *db=0;
	if (tree_fn==0 || clusters_fn ==0) {
		int rc = sqlite3_open(db_fn, &db);
		if (rc) {
			cerr << db_fn << ": " << sqlite3_errmsg(db) << endl;
			return -1;
		}
	}

	kmean_tree::node_t *root = (db ? kmean_tree::load(db) : kmean_tree::load(tree_fn));
	if (!root) {
		std::cerr << tree_fn << ": unable to load tree\n";
		if (db) sqlite3_close(db);
		return -1;
	}
	
	id_cluster_collection *clusters = new id_cluster_collection(id_cluster_collection::QUERY_NORMALIZED_FREQ);

	for (int i=1; i<argc; i++) {
		if (i<argc-1) {
			if (strcmp(argv[i],"-d")==0) {
				if (!load_patch_track(clusters, argv[++i], root)) {
					cerr << argv[i] << ": loading failed.\n";
				} else {
					cerr << argv[i] << ": loaded successfully.\n";
				}
			} else if (strcmp(argv[i],"-C")==0) {
				sqlite3 *db;
				const char *db_fn = argv[++i];
				int rc = sqlite3_open_v2(db_fn, &db, SQLITE_OPEN_READONLY, 0);
				if (rc) {
					cerr << db_fn << ": can't open database\n";
				} else {
				       	if (!clusters->load(db))
						cerr << db_fn << ": can't read clusters in database.\n";
					sqlite3_close(db);
				}
			} else if (strcmp(argv[i],"-c")==0) {

				if (!clusters->load(argv[++i])) {
					cerr << argv[i] << ": loading failed.\n";
				} else {
					cerr << argv[i] << ": loaded successfully.\n";
				}
			}
		}
	}


	if (clusters->clusters.size() == 0) {
		cerr << "Nothing loaded!\n";
		if (db) sqlite3_close(db);
		return -1;
	}
	cout << clusters->clusters.size() << " clusters loaded." << endl;

	if (threshold>=0)
		clusters->reduce(threshold);

	//clusters->print();
	if (!clusters_fn && db) {
		clusters->save(db);
		sqlite3_close(db);
	} else {
		clusters->save(clusters_fn);
	}

	return 0;
}
