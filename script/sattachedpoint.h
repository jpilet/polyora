#ifndef SATTACHEDPOINT_H
#define SATTACHEDPOINT_H

#include "spoint.h"
#include "softmatch.h"
#include <QtScript>

class SModel;

class SAttachedPoint : public SPoint
{
    Q_OBJECT
    Q_PROPERTY(SPoint * src READ getSrc)
    Q_PROPERTY(bool isAttached READ isAttached)

public:
    SAttachedPoint();
    SAttachedPoint(SModel *m, double x, double y);
    SAttachedPoint(SModel *m);

    Q_INVOKABLE bool attach(SModel *m, double x, double y);

    bool isAttached() { return attached && model!=0; }
    SPoint *getSrc() { return &src; }

    static void installInEngine(QScriptEngine *engine);

public slots:
    void attachFromImage(float newx, float newy);

    void update();
    void moveSource(float newx, float newy);
    void detach();

protected:
    SPoint src;
    SoftMatcher::TrigModelPoint mp;
    SModel *model;
    bool attached;
};

Q_DECLARE_METATYPE(SAttachedPoint *)

#endif // SATTACHEDPOINT_H
