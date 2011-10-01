#include "smodel.h"
#include "sattachedpoint.h"

std::map<QString, SModel *> SModel::models;

SModel::SModel(SoftSnake *model, QGLWidget *context) : snake(model), context(context)
{
    timeout = 5;
    if (strcmp(model->texname,"script")==0) {
        model->snake->softMatcher->useSTexture=true;
        model->snake->softMatcher->setSTexture( new STexture(context,0,false), true);
    }
    lost=true;
}

SModel::~SModel()
{
    snake->snake->softMatcher->setSTexture(0,true);
}

bool SModel::isDetected() const { return snake->success(); }

void SModel::update()
{
    if (snake->success()) {
        lost=false;
        if (last_seen.isNull()) {
            last_seen.start();
            last_appeared.start();
            emit appeared();
        } else if (last_seen.restart() > (int)(timeout*1000.0)) {
            last_appeared.restart();
            emit appeared();
        }
        emit moved();
    } else {
        if (!lost && (last_seen.elapsed() > (int)(timeout*1000.0))) {
            lost = true;
            emit disappeared();
        }
    }
}

double SModel::timeSinceLastSeen() const
{
    if (last_seen.isNull()) return -1;
    return last_seen.elapsed() / 1000.0;
}

double SModel::timeSinceLastAppeared() const
{
    if (last_appeared.isNull()) return -1;
    return last_appeared.elapsed() /1000.0;
}

void SModel::addModel(const char *name, SoftSnake *s, QGLWidget *context)
{
    models[QString(name)] = new SModel(s, context);
}

SModel *SModel::findModel(const QString &name)
{
    std::map<QString, SModel *>::iterator it = models.find(name);
    if (it == models.end()) return 0;
    return it->second;
}

void SModel::updateAll()
{
    for (std::map<QString, SModel *>::iterator it = models.begin();
        it!=models.end(); ++it)
    {
        it->second->update();
    }
}

void SModel::resetAll(QGLWidget *context)
{
    for (std::map<QString, SModel *>::iterator it = models.begin();
        it!=models.end(); ++it)
    {
        SoftSnake *snake = it->second->snake;
        delete it->second;
        it->second = new SModel(snake, context);
    }
}

// DIRTY HACK
extern double input_image_height;
void SModel::display()
{
    context->makeCurrent();
    glMatrixMode(GL_MODELVIEW);
    float currentColor[4];
    glGetFloatv(GL_CURRENT_COLOR, currentColor);
    glPushMatrix();
    glTranslated(0, input_image_height, 0);
    glScalef(1,-1,1);
    snake->display();
    glPopMatrix();
    glColor4fv(currentColor);
}

static int getPointArg(int arg, double *xy, QScriptContext *context, QScriptEngine *engine)
{
    if (context->argument(arg).isNumber() && context->argument(arg+1).isNumber()) {
        xy[0] = context->argument(arg).toNumber();
        xy[1] = context->argument(arg+1).toNumber();
        return arg+2;
    } else {
        QScriptValue _x = context->argument(arg).property("x");
        QScriptValue _y = context->argument(arg).property("y");
        if (!_x.isNumber() || !_y.isNumber() ) {
            context->throwError("Type Error: argument should be a 2d point");
            return -1;
        }
        xy[0] = _x.toNumber();
        xy[1] = _y.toNumber();
        return arg+1;
    }
}

static QScriptValue attachFromImage(QScriptContext *context, QScriptEngine *engine)
 {
    if (context->argumentCount() < 1) {
        context->throwError("not enough arguments");
         return engine->nullValue();
    }

    SModel *m = qobject_cast<SModel *>(context->thisObject().toQObject());
    if (!m) {
        context->throwError("wrong type for 'this'");
        return engine->nullValue();
    }

    double xy[2];
    if (getPointArg(0,xy,context,engine) < 0) return engine->nullValue();

    SAttachedPoint *p = new SAttachedPoint(m);
    p->attachFromImage(xy[0], xy[1]);
    return engine->newQObject(p, QScriptEngine::ScriptOwnership);
}

static QScriptValue attach(QScriptContext *context, QScriptEngine *engine)
 {
    if (context->argumentCount() < 1) {
        context->throwError("not enough arguments");
         return engine->nullValue();
    }

    SModel *m = qobject_cast<SModel *>(context->thisObject().toQObject());
    if (!m) {
        context->throwError("wrong type for 'this'");
        return engine->nullValue();
    }

    double xy[2];
    if (getPointArg(0,xy,context,engine)<0) return engine->nullValue();

    return engine->newQObject(new SAttachedPoint(m,xy[0],xy[1]),
                              QScriptEngine::ScriptOwnership);
}

static QScriptValue sFindModel(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() < 1) {
         return engine->nullValue();
    }

    QString name = context->argument(0).toString();
    SModel *m = SModel::findModel(name);
    QScriptValue v = engine->newQObject(m);
    v.setProperty("attach", engine->newFunction(attach, 2));
    v.setProperty("attachFromImage", engine->newFunction(attachFromImage,2));
    return v;
}


QScriptValue smodelToScriptValue(QScriptEngine *engine, SModel* const  &in)
{
     QScriptValue v = engine->newQObject(in);
     v.setProperty("attach", engine->newFunction(attach, 2));
     return v;
}

void smodelFromScriptValue(const QScriptValue &object, SModel * &out)
{
    out = qobject_cast<SModel *>(object.toQObject());
}

void SModel::installInEngine(QScriptEngine *engine)
{
    engine->globalObject().setProperty("findModel", engine->newFunction(sFindModel, 1));
    qScriptRegisterMetaType(engine, smodelToScriptValue, smodelFromScriptValue);
}

void SModel::attachPoint(SAttachedPoint *p, float x, float y)
{
    p->attach(this, x, y);
}
