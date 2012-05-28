#ifndef SGRAPHICS_H
#define SGRAPHICS_H

#include <QObject>
#include <QtScript>
#include <list>
#include "spoint.h"
#include <QGLWidget>
#include "stexture.h"

class SGraphics : public QObject
{
    Q_OBJECT

    Q_PROPERTY(STexture * texture READ getTexture WRITE setTexture);

public:
    SGraphics(QGLWidget *context, QObject* parent);

    void installInEngine(QScriptEngine *engine);
    void update() { emit onPaint(); }

    STexture *getTexture() { return currentTexture; }
    STexture *setTexture(STexture *tex) { return currentTexture = tex; }

signals:
    void onPaint();

public slots:
    void drawQuad(const SPoint *a, const SPoint *b, const SPoint *c, const SPoint *d);
    void drawQuad(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
    void setColor(double r, double g, double b, double a=1);
    void pushColor(double r, double g, double b, double a=1) { colorStack.push_front(Color(r,g,b,a)); setColor(r,g,b,a); }
    void pushLineWidth(float width) { lineWidthStack.push(width); glLineWidth(width); }
    void popLineWidth() { lineWidthStack.pop_front(); glLineWidth(lineWidthStack.front()); }
    void popColor() { colorStack.pop_front(); setColor(colorStack.front());  }
    void pushMatrix() { glPushMatrix(); }
    void popMatrix() { glPopMatrix(); }
    void translate(double x, double y) { glTranslated(x,y,0); }
    void scale(double s) { glScaled(s, s, 1); }
    void scale(double s, double t) { glScaled(s, t, 1); }
    void translate(const SPoint *p) { glTranslatef(p->getX(),p->getY(),0); }
    void rotate(double angle, double x, double y) { glTranslated(x,y,0); glRotated(angle, 0,0,1); glTranslated(-x,-y,0); }

private:
    QGLWidget *context;
    STexture *currentTexture;

    struct Color {
        double r,g,b,a;
        Color(double r, double g, double b, double a) : r(r), g(g), b(b), a(a) {}
    };
    std::list<Color> colorStack;
    std::list<float> lineWidthStack;
    void setColor(const Color &c) { setColor(c.r, c.g, c.b, c.a); }

    static QScriptValue createTexture(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue homography(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue drawRectangle(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue drawLineStrip(QScriptContext *context, QScriptEngine *engine);
};

#endif // SGRAPHICS_H
