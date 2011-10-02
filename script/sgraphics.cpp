#include <QtOpenGL>
#include "sgraphics.h"
#include "homography.h"

SGraphics::SGraphics(QGLWidget *context, QObject* parent)
    : QObject(parent), context(context)
{
    currentTexture=0;
    pushColor(1,1,1,1);
}

void SGraphics::drawQuad(const SPoint *a, const SPoint *b, const SPoint *c, const SPoint *d)
{
    drawQuad(a->getX(), a->getY(),
             b->getX(), b->getY(),
             c->getX(), c->getY(),
             d->getX(), d->getY());
}

static void setupHomography(const float H[3][3])
{
        float m[4][4];

        glMatrixMode(GL_MODELVIEW);
        //glLoadIdentity();
        m[0][0]=H[0][0];  m[1][0]=H[0][1];  m[2][0]=0;  m[3][0]=H[0][2];
        m[0][1]=H[1][0];  m[1][1]=H[1][1];  m[2][1]=0;  m[3][1]=H[1][2];
        m[0][2]=0;        m[1][2]=0;        m[2][2]=1;  m[3][2]=0;
        m[0][3]=H[2][0];  m[1][3]=H[2][1];  m[2][3]=0;  m[3][3]=H[2][2];

        glMultMatrixf(&m[0][0]);
        // homographic transform is now correctly loaded. Following
}

QScriptValue SGraphics::homography(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() != 1 || !context->argument(0).isArray()) {
        context->throwError("TypeError");
        return engine->undefinedValue();
    }

    float r[16];
    for (int i=0; i<16; i++) {
        r[i] = (float) (context->argument(0).property(i, QScriptValue::ResolveLocal).toNumber());
    }
    float H[3][3];
    script_homography::homography_from_4corresp(r+0, r+2, r+4, r+6, r+8, r+10, r+12, r+14, H);

    glPushMatrix();
    setupHomography(H);
    return engine->undefinedValue();
}

QScriptValue SGraphics::drawRectangle(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() < 1) {
        context->throwError("Not enough arguments");
        return engine->undefinedValue();
    }


    SGraphics *_this = qobject_cast<SGraphics *>(context->thisObject().toQObject());
    if (!_this) {
        context->throwError("'this' is supposed to be the 'graphics' object");
        return engine->undefinedValue();
    }
    STexture *currentTexture = _this->currentTexture;

    if (currentTexture && currentTexture->isValid()) {
        currentTexture->bind();
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);

    for (int a=0; a<context->argumentCount();a++) {

        QScriptValue q = context->argument(a);
        double x = q.property("x", QScriptValue::ResolveLocal).toNumber();
        double y = q.property("y", QScriptValue::ResolveLocal).toNumber();
        double w = q.property("w", QScriptValue::ResolveLocal).toNumber();
        double h = q.property("h", QScriptValue::ResolveLocal).toNumber();

        if (currentTexture && currentTexture->isValid()) {
            SPoint *p = currentTexture->getRatioCoord(0);
            glTexCoord2f(p->getX(), p->getY());
            glVertex2d(x,y);
            p = currentTexture->getRatioCoord(3);
            glTexCoord2f(p->getX(), p->getY());
            glVertex2d(x,y+h);
            p = currentTexture->getRatioCoord(2);
            glTexCoord2f(p->getX(), p->getY());
            glVertex2d(x+w,y+h);
            p = currentTexture->getRatioCoord(1);
            glTexCoord2f(p->getX(), p->getY());
            glVertex2d(x+w,y);

        } else {
            glVertex2d(x,y);
            glVertex2d(x,y+h);
            glVertex2d(x+w,y+h);
            glVertex2d(x+w,y);
        }
    }
    glEnd();
    return engine->undefinedValue();
}

void SGraphics::drawQuad(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
{
    context->makeCurrent();
    float H[3][3];
    float p1[] = {(float)x1,(float)y1};
    float p2[] = {(float)x2,(float)y2};
    float p3[] = {(float)x3,(float)y3};
    float p4[] = {(float)x4,(float)y4};
    script_homography::homography_from_4pt(p1,p2,p3,p4,&H[0][0]);

    glPushMatrix();
    setupHomography(H);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (currentTexture && currentTexture->isValid()) {
        currentTexture->bind();
        glBegin(GL_QUADS);
        SPoint *p = currentTexture->getRatioCoord(0);
        glTexCoord2f(p->getX(), p->getY());
        glVertex2d(0,0);
        p = currentTexture->getRatioCoord(1);
        glTexCoord2f(p->getX(), p->getY());
        glVertex2d(0,1);
        p = currentTexture->getRatioCoord(2);
        glTexCoord2f(p->getX(), p->getY());
        glVertex2d(1,1);
        p = currentTexture->getRatioCoord(3);
        glTexCoord2f(p->getX(), p->getY());
        glVertex2d(1,0);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        glBegin(GL_QUADS);
        glVertex2d(0,0);
        glVertex2d(0,1);
        glVertex2d(1,1);
        glVertex2d(1,0);
        glEnd();
    }
    glPopMatrix();
}

void SGraphics::setColor(double r, double g, double b, double a)
{
    context->makeCurrent();
    glColor4d(r,g,b,a);
    if (colorStack.size()>0) colorStack.front() = Color(r,g,b,a);
}

QScriptValue SGraphics::createTexture(QScriptContext *context, QScriptEngine *engine)
{
    SGraphics *_this = qobject_cast<SGraphics *>(context->thisObject().toQObject());
    if (_this==0) {
        return context->throwError(QScriptContext::TypeError, "SGraphics.createTexture: invalid type of 'this' object");
    }
    STexture *tex = new STexture(_this->context);

    if (context->argumentCount() >= 1) {
        QString name = context->argument(0).toString();
        if (!tex->loadDDS(name)) {
            return context->throwError(QScriptContext::URIError, name + ": can't load dds file.");
        }
    }

    return engine->newQObject(tex,QScriptEngine::ScriptOwnership);
}

void SGraphics::installInEngine(QScriptEngine *engine)
{
    QScriptValue v = engine->newQObject(this, QScriptEngine::QtOwnership);
    v.setProperty("createTexture", engine->newFunction(createTexture,1));
    v.setProperty("homography", engine->newFunction(homography,1));
    v.setProperty("drawRectangle", engine->newFunction(drawRectangle,1));
    engine->globalObject().setProperty("graphics", v);
}
