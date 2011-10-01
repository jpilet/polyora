#include "spoint.h"

static QScriptValue plus(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() == 0) {
        return context->thisObject();
    }
    SPoint *_this = qobject_cast<SPoint *>(context->thisObject().toQObject());
    if (!_this) {
        return context->throwError(QScriptContext::TypeError, "SPoint.plus: invalid type of 'this' object");
    }
    float x= _this->getX();
    float y= _this->getY();
    for (int i=0; i<context->argumentCount(); ++i) {
        SPoint *a = qobject_cast<SPoint *>(context->argument(i).toQObject());
        if (a) {
            x+=a->getX();
            y+=a->getY();
        } else {
            if (i+1 < context->argumentCount()) {
                x+=(float)context->argument(i).toNumber();
                y+=(float)context->argument(i+1).toNumber();
                i++;
            } else
                return context->throwError(QScriptContext::TypeError, "SPoint.plus: invalid number or type of argument");
        }
    }
    return engine->newQObject(new SPoint(x,y), QScriptEngine::ScriptOwnership);
}

static QScriptValue minus(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() == 0) {
        return context->thisObject();
    }
    SPoint *_this = qobject_cast<SPoint *>(context->thisObject().toQObject());
    if (!_this) {
        return context->throwError(QScriptContext::TypeError, "SPoint.minus: invalid type of 'this' object");
    }
    float x= _this->getX();
    float y= _this->getY();
    for (int i=0; i<context->argumentCount(); ++i) {
        SPoint *a = qobject_cast<SPoint *>(context->argument(i).toQObject());
        if (a) {
            x-=a->getX();
            y-=a->getY();
        } else {
            if (i+1 < context->argumentCount()) {
                x-=(float)context->argument(i).toNumber();
                y-=(float)context->argument(i+1).toNumber();
                i++;
            } else
                return context->throwError(QScriptContext::TypeError, "SPoint.minus: invalid number or type of argument");
        }
    }
    return engine->newQObject(new SPoint(x,y), QScriptEngine::ScriptOwnership);
}

static QScriptValue times(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() != 1) {
        return context->throwError(QScriptContext::TypeError, "SPoint.times: invalid number of arguments");
    }
    SPoint *_this = qobject_cast<SPoint *>(context->thisObject().toQObject());
    if (!_this) {
        return context->throwError(QScriptContext::TypeError, "SPoint.times: invalid type of 'this' object");
    }
    float x= _this->getX();
    float y= _this->getY();

    float mul =(float)context->argument(0).toNumber();

    return engine->newQObject(new SPoint(mul*x,mul*y), QScriptEngine::ScriptOwnership);
}

static QScriptValue spointToString(QScriptContext *context, QScriptEngine *engine)
{
    SPoint *_this = qobject_cast<SPoint *>(context->thisObject().toQObject());
    if (!_this) {
        return context->throwError(QScriptContext::TypeError, "SPoint.plus: invalid type of 'this' object");
    }
     QString s = QString("(%1, %2)").arg(_this->getX()).arg(_this->getY());
     return QScriptValue(engine, s);
}

static QScriptValue newPoint(QScriptContext *context, QScriptEngine *engine)
{
     float x=0,y=0;
     if (context->argumentCount() > 1) {
         x = float(context->argument(0).toNumber());
         y = float(context->argument(1).toNumber());
     }
     return engine->newQObject(new SPoint(x,y), QScriptEngine::ScriptOwnership);
}

QScriptValue spointToScriptValue(QScriptEngine *engine, SPoint* const  &in)
{ return engine->newQObject(in); }

 void spointFromScriptValue(const QScriptValue &object, SPoint * &out)
 { out = qobject_cast<SPoint *>(object.toQObject()); }


void SPoint::installInEngine(QScriptEngine *engine)
{
    engine->globalObject().setProperty("newPoint", engine->newFunction(newPoint, 2));

    QScriptValue prototype = engine->newObject();
    prototype.setProperty("plus", engine->newFunction(plus,2));
    prototype.setProperty("minus", engine->newFunction(minus,2));
    prototype.setProperty("times", engine->newFunction(times,1));
    prototype.setProperty("toString", engine->newFunction(spointToString, 0));
    qScriptRegisterMetaType(engine, spointToScriptValue, spointFromScriptValue, prototype);
}
