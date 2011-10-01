#ifndef STIME_H
#define STIME_H

#include <QObject>
#include <QtScript>
#include <QTime>

class STime : public QObject
{
    Q_OBJECT

    //Q_PROPERTY(STexture * texture READ getTexture WRITE setTexture);
    Q_PROPERTY(bool running READ isRunning WRITE setRunning);

public:
    STime(QObject *parent=0) : QObject(parent) { running=false; time.start(); }

    static void installInEngine(QScriptEngine *engine);

    bool isRunning() { return running; }
    bool setRunning(bool r) { if (running) stop(); else cont(); return running; }

public slots:
    double elapsed() {
        if (running) { return _elapsed + time.elapsed() / 1000.0; }
        else return _elapsed;
    }
    double restart() {
        double r = _elapsed;
        if (running) { r+= time.restart()/1000.0;}
        else {
            running = true;
            time.start();
        }
        _elapsed=0;
        return r;
     }
    void start() { _elapsed=0; time.restart(); running=true; }
    void stop() { if (running) { _elapsed = elapsed(); running=false; } }
    void pause() { stop(); }
    double stopReset() { double e=elapsed(); running=false; _elapsed=0; return e; }
    void cont() { if (!running) { time.restart(); running=true; } }

    void startFrom(double t) { running=true; _elapsed=t; time.restart(); }

private:
    QTime time;
    bool running;
    double _elapsed;
};

Q_DECLARE_METATYPE(STime *)

#endif // STIME_H
