#include <iostream>
#include "spolyora_target.h"
#include "sattachedpoint.h"

SAttachedPoint::SAttachedPoint()
{
    model=0;
    QObject::connect(&src, SIGNAL(moved(float, float)), this, SLOT(moveSource(float, float)));
}

SAttachedPoint::SAttachedPoint(SPolyoraTarget *m, double x, double y)
{
    model=0;
    QObject::connect(&src, SIGNAL(moved(float, float)), this, SLOT(moveSource(float, float)));
    attach(m,x,y);
}

SAttachedPoint::SAttachedPoint(SPolyoraTarget *m)
{
    model=m;
    QObject::connect(model, SIGNAL(moved()), this, SLOT(update()));
    QObject::connect(&src, SIGNAL(moved(float, float)), this, SLOT(moveSource(float, float)));
}

bool SAttachedPoint::attach(SPolyoraTarget *m, double x, double y)
{
    detach();

    if (!m) return false;

    model = m;
    QObject::connect(model, SIGNAL(moved()), this, SLOT(update()));

    src.move(x,y);

    return isAttached();
}

void SAttachedPoint::detach()
{
    if (!model) return;
    QObject::disconnect(model, SIGNAL(moved()), this, SLOT(update()));
    model=0;
}

void SAttachedPoint::update()
{
    if (!isAttached()) return;
    model->transformPoint(&src, this);
}

void SAttachedPoint::moveSource(float x, float y)
{
    update();
}

void SAttachedPoint::attachFromImage(float x, float y)
{
    SPoint::move(x,y);

    if (!model) return;
    model->inverseTransformPoint(this, &src);
}

QScriptValue sAttachedPointToScriptValue(QScriptEngine *engine, SAttachedPoint* const  &in)
{
     return engine->newQObject(in);
}

void sAttachedPointFromScriptValue(const QScriptValue &object, SAttachedPoint * &out)
{
    out = qobject_cast<SAttachedPoint *>(object.toQObject());
}

void SAttachedPoint::installInEngine(QScriptEngine *engine)
{
    QScriptValue prototype = engine->defaultPrototype(qMetaTypeId<SPoint *>());

    qScriptRegisterMetaType(engine, sAttachedPointToScriptValue, sAttachedPointFromScriptValue,
                            prototype);

    engine->globalObject().setProperty("newAttachedPoint",
        engine->newFunction(newAttachedPoint, 3));
}
bool SAttachedPoint::isValid() const {
    return model!=0 && model->isDetected();
}

QScriptValue SAttachedPoint::newAttachedPoint(QScriptContext *context, QScriptEngine *engine)
{
     SPolyoraTarget* target = 0;
     float x=0,y=0;

     if (context->argumentCount() >= 1) {
       target = qobject_cast<SPolyoraTarget *>(context->argument(0).toQObject());
     }
     if (context->argumentCount() >= 3) {
         x = float(context->argument(1).toNumber());
         y = float(context->argument(2).toNumber());
     }
     return engine->newQObject(new SAttachedPoint(target, x, y),
         QScriptEngine::ScriptOwnership);
}
