#ifndef SATTACHEDPOINT_H
#define SATTACHEDPOINT_H

#include "spoint.h"
#include <QtScript>

class SPolyoraTarget;

class SAttachedPoint : public SPoint
{
    Q_OBJECT
    Q_PROPERTY(SPoint * src READ getSrc)
    Q_PROPERTY(bool isAttached READ isAttached)
    Q_PROPERTY(bool isValid READ isValid)

public:
    SAttachedPoint();
    SAttachedPoint(SPolyoraTarget *m, double x, double y);
    SAttachedPoint(SPolyoraTarget *m);

    Q_INVOKABLE bool attach(SPolyoraTarget *m, double x, double y);

    bool isAttached() const { return model!=0; }
    bool isValid() const;
    SPoint *getSrc() { return &src; }

    static void installInEngine(QScriptEngine *engine);
    static QScriptValue newAttachedPoint(QScriptContext *context, QScriptEngine *engine);

public slots:
    void attachFromImage(float newx, float newy);

    void update();
    void moveSource(float newx, float newy);
    void detach();

protected:
    SPoint src;
    SPolyoraTarget *model;
};

Q_DECLARE_METATYPE(SAttachedPoint *)

#endif // SATTACHEDPOINT_H
