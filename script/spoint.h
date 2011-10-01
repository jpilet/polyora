#ifndef SPOINT_H
#define SPOINT_H

#include <QObject>
#include <QtScript>

class SPoint : public QObject
{
    Q_OBJECT
    Q_PROPERTY(float x READ getX WRITE setX)
    Q_PROPERTY(float y READ getY WRITE setY)

public:
    SPoint(float x, float y) : x(x), y(y) {}
    SPoint() : x(0), y(0) {}

signals:
    void moved(float newx, float newy);

public slots:
    virtual void move(float newx, float newy) {
        emit moved(newx, newy);
        x = newx;
        y = newy;
    }
    void add(const SPoint *a) { move(x+a->x, y+a->y); }
    void add(float a, float b) { move(x+a, y+b); }
    void sub(const SPoint *a) { move(x - a->x, y - a->y); }
    void sub(float a, float b) { move(x-a, y-b); }
    void mul(double a) { move(a*x, a*y); }
    double dist(const SPoint *a) const {
        float dx=x-a->x;
        float dy=y-a->y;
        return sqrt((double)(dx*dx+dy*dy));
    }
    double norm() { return sqrt(x*x+y*y); }

public:
    float getX() const { return x; }
    float getY() const { return y; }
    void setX(float newX) { emit moved(newX, y); x = newX; }
    void setY(float newY) { emit moved(x, newY); y = newY; }
    void swap(SPoint &a) {
        float ax = a.x;
        float ay = a.y;
        a.move(x,y);
        move(ax,ay);
    }

    static void installInEngine(QScriptEngine *engine);
private:
    float x,y;
};

Q_DECLARE_METATYPE(SPoint *)
#endif // SPOINT_H
