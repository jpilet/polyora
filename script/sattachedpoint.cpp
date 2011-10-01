#include <iostream>
#include "smodel.h"
#include "sattachedpoint.h"

SAttachedPoint::SAttachedPoint()
{
    model=0;
    attached = false;
    QObject::connect(&src, SIGNAL(moved(float, float)), this, SLOT(moveSource(float, float)));
}

SAttachedPoint::SAttachedPoint(SModel *m, double x, double y)
{
    model=0;
    attached = false;
    QObject::connect(&src, SIGNAL(moved(float, float)), this, SLOT(moveSource(float, float)));
    attach(m,x,y);
}

SAttachedPoint::SAttachedPoint(SModel *m)
{
    model=m;
    QObject::connect(model, SIGNAL(moved()), this, SLOT(update()));
    attached = false;
    QObject::connect(&src, SIGNAL(moved(float, float)), this, SLOT(moveSource(float, float)));
}
// DIRTY HACK
extern double input_image_height;

bool SAttachedPoint::attach(SModel *m, double x, double y)
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
    attached=false;
}

void SAttachedPoint::update()
{
    if (!isAttached()) return;
    mp.transform(model->snake->UVWs);
    move(mp.tpos.x, input_image_height-mp.tpos.y);
    //move(mp.tpos.x, mp.tpos.y);
}

void SAttachedPoint::moveSource(float x, float y)
{
    if (!model) return;
    SoftMatcher *softmatcher = model->snake->snake->softMatcher;
    int model_height = softmatcher->model->Image.RImage->ydim;

    mp.pos.set( x,model_height-y);

    attached = softmatcher->pickFacetAndCalcWeights(model->snake->baseUVWs, mp);

    if (attached) {
        update();
    } else {
        std::cerr << "can't attach point "
                                << x << ", " << y
                                << " to mesh!\n";
    }
}

void SAttachedPoint::attachFromImage(float x, float y)
{
    SPoint::move(x,y);

    if (!model) return;
    if (attached) detach();

    SoftMatcher *softmatcher = model->snake->snake->softMatcher;
    int input_height = input_image_height;
    int model_height = softmatcher->model->Image.RImage->ydim;

    mp.pos.set( x,input_height-y);

    attached = softmatcher->pickFacetAndCalcWeights(model->snake->UVWs, mp);

    if (attached) {
        mp.transform(model->snake->baseUVWs);
        //std::cout << " back transformed point: " << mp.tpos.x << ", " << mp.tpos.y << std::endl;
        src.move(mp.tpos.x, model_height-mp.tpos.y);
        update();
    } /*else {
        std::cerr << "can't attach point "
                                << x << ", " << y
                                << " to mesh!\n";
    }*/
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
}
