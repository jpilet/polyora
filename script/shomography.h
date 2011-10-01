#ifndef SHOMOGRAPHY_H
#define SHOMOGRAPHY_H

#include "sattachedpoint.h"

class SHomography : public QObject
{
	Q_OBJECT

public:

		SHomography(SAttachedPoint *a=0, SAttachedPoint *b=0, SAttachedPoint *c=0, SAttachedPoint *d=0);

		static void installInEngine(QScriptEngine *engine);

public slots:
		void setup(SAttachedPoint *a, SAttachedPoint *b, SAttachedPoint *c, SAttachedPoint *d);
		SPoint *transform(const SPoint *src);
		SPoint *get(int i);
		void update() { dirty=true; }
		void update(float, float) { dirty=true; }

private:
		bool dirty;
		SAttachedPoint *pts[4];
		double H[3][3];
};

Q_DECLARE_METATYPE(SAttachedPoint *)

#endif
