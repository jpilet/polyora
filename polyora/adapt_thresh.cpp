/*! \author Hideaki Uchiyama
 */
#include <cv.h>

#include <vector>
using namespace std;

// img: input color image
// result: feature area described by binary (should be allocated beforehand) 
void ExtractKptArea(const IplImage* img, IplImage* result)
{
	cvSmooth(img, result, CV_GAUSSIAN, 3);	// filter size is 3
	cvAdaptiveThreshold(result, result, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY_INV, 9);	// filter size is 9
	cvDilate(result, result, NULL, 1);
}

// area: binary
// result: labeling result
int	Labeling(const IplImage* area, int *result)
{
	int nsort[5];
	int x, y, i, flag, countk, label, p;
	int labelnumber;
	int labelnum = -1;
	int map[50000];			// 50000 is meaningless

	map[0] = 0;

	int imgw = area->width;
	int imgh = area->height;
 
	// copy image
	for(y=1;y<imgh-1;y++){
		unsigned char *l = &CV_IMAGE_ELEM(area, unsigned char, y, 0);
		for(x=1;x<imgw-1;x++){
			result[x+y*imgw] = *l++;
		}
	}

	// set 0 on image border
	for(x=0;x<imgw;x++){
		result[x] = result[x+(imgh-1)*imgw] = 0;
	}
	for(y=0;y<imgh;y++){
		result[0+y*imgw] = result[(imgw-1)+y*imgw] = 0;
	}

	// start
	label = 0;
	for(y=1;y<imgh-1;y++){
		for(x=1;x<imgw-1;x++){
			map[0] = 0;

			if(result[x+y*imgw] == 0){
				continue;
			}

			nsort[0] = 0;
			nsort[1] = result[(x-1)+(y-1)*imgw];
			nsort[2] = result[x+(y-1)*imgw];
			nsort[3] = result[(x+1)+(y-1)*imgw];
			nsort[4] = result[(x-1)+y*imgw];

			for(flag = 1; flag == 1;){
				for(flag = 0, i = 2; i < 5; i++){
					if (nsort[i] < nsort[i-1]){
						countk = nsort[i];
						nsort[i] = nsort[i-1];
						nsort[i-1] = countk;
						flag = 1;
					}
				}
			}
			countk = 0;
			for(i = 1; i < 5; i++){
				if(nsort[i] == 0){
					continue;
				}
				if(nsort[i] == nsort[i-1]){
					continue;
				}
				countk++;
				nsort[countk] = nsort[i];
			}

			i = countk;
			if(i == 0){
				label++;
				result[x+y*imgw] = label;
				map[label] = label;
			}
			else if(i == 1){
				countk = nsort[1];
				while(map[countk] != countk){
					countk = map[countk];
				}
				result[x+y*imgw] = countk;
			}
			else if(i > 1){
				countk = nsort[1];
				while(map[countk] != countk){
					countk = map[countk];
				}
				result[x+y*imgw] = countk;
				for (p = 2; p <= i; p++){
					map[nsort[p]] = countk;
				}
			}		
		}
	}

	labelnum=0;
	for(labelnumber=1;labelnumber<=label;labelnumber++){
		if(map[labelnumber] == labelnumber){
			labelnum++;                       
			map[labelnumber] = labelnum;
		}
		else{
			map[labelnumber] = map[map[labelnumber]];
		}
	}

	for(y=1; y<imgh-1; y++){
		for(x=1; x<imgw-1; x++){
			labelnumber = result[x+y*imgw];
			if(labelnumber == 0){
				continue;
			}
			result[x+y*imgw] = map[labelnumber];
		}
	}

	return labelnum+1;
}

struct pt
{
	pt():sumx(.0f), sumy(.0f), count(0) {}
	float sumx, sumy;
	int count;
};


// label: labeling
// numlabel: number of labels
// kpt: result
int ExtractKpt(const int *label, const int numlabel, const int imgw, const int imgh, CvMat *kpt)
{
	vector<pt> blobs(numlabel+1);

	for(int y=1; y<imgh-1; y++){
		for(int x=1; x<imgw-1; x++){
			if(label[x+y*imgw] == 0){
				continue;
			}

			blobs[label[x+y*imgw]].sumx += static_cast<float>(x);
			blobs[label[x+y*imgw]].sumy += static_cast<float>(y);
			blobs[label[x+y*imgw]].count++;

		}
	}

	int numkpt = 0;
	for(int i=1;i<static_cast<int>(blobs.size());i++){	// 0 is background
		if(blobs[i].count > 10){	// remove small areas
			float ptx, pty;
			ptx = blobs[i].sumx / static_cast<float>(blobs[i].count);
			pty = blobs[i].sumy / static_cast<float>(blobs[i].count);

			cvmSet(kpt,numkpt,0,ptx);
			cvmSet(kpt,numkpt,1,pty);

			numkpt++;
		}
	}

	return numkpt;
}

int detectAdaptiveTresholdKeypoints(const IplImage *img, CvMat *kpt)
{
	// feature area (grayscale)
	IplImage* area = cvCreateImage(cvGetSize(img),8,1);

	// extract feature area
	ExtractKptArea(img, area);

	// label img
	int *label = new int[img->width*img->height];	// size should be same as image
	int numlabel = Labeling(area, label);

	int numkpt = ExtractKpt(label, numlabel, img->width, img->height, kpt);

	cvReleaseImage(&area);
	delete [] label;

	return numkpt;
}
