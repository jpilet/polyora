#include "shomography.h"

SHomography::SHomography(SAttachedPoint *a=0, SAttachedPoint *b=0, SAttachedPoint *c=0, SAttachedPoint *d=0)
{
	for (int i=0; i<4; i++) pts[i]=0;
	setup(a,b,c,d);
}


void SHomography::setup(SAttachedPoint *a, SAttachedPoint *b, SAttachedPoint *c, SAttachedPoint *d)
{
	dirty = true;
	for (int i=0; i<4; i++)
		if (pts[i]) disconnect(pts[i], SLOT(update(float,float)));
	pts[0] = a;
	pts[1] = b;
	pts[2] = c;
	pts[3] = d;
	for (int i=0; i<4; i++)
		if (pts[i]) connect(pts[i], SIGNAL(moved(float,float)), SLOT(update(float,float)));
}

SPoint *SHomography::transform(const SPoint *src) {
	if (dirty) {
		if (!cmpHomography()) return 0;
	}
	double a = H[0][0] * src->getX() + H[0][1]*src->getY() + H[0][2];
	double b = H[1][0] * src->getX() + H[1][1]*src->getY() + H[1][2];
	double c = H[2][0] * src->getX() + H[2][1]*src->getY() + H[2][2];
	return new SPoint(a/c, b/c); // is it leaked ?
}

		SPoint *get(int i);
		void update();
