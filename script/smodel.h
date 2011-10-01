#ifndef SMODEL_H
#define SMODEL_H

#include <map>
#include <QObject>
#include <QtScript>
#include <QString>
#include <QtOpenGL>
#include "stexture.h"
#include "softsnake.h"

class SAttachedPoint;

class SModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool detected READ isDetected)
    Q_PROPERTY(double sinceLastSeen READ timeSinceLastSeen)
    Q_PROPERTY(double sinceLastAppeared READ timeSinceLastAppeared)
    Q_PROPERTY(double disappearTimeout READ getDisappearTimeout WRITE setDisappearTimeout)
    Q_PROPERTY(STexture *texture READ getTexture WRITE setTexture)
    Q_PROPERTY(bool enableDetection READ getEnableDetection WRITE setEnableDetection)
    Q_PROPERTY(bool enableReshader READ getReshader WRITE setReshader)
    Q_PROPERTY(bool enableDisplay READ getDisplayFlag WRITE setDisplayFlag)

signals:
    void moved();
    void appeared(); // a move signal will always follow an appeared signal.
    void disappeared();

public:
    SModel(SoftSnake *model, QGLWidget *context);
    ~SModel();

    bool isDetected() const;

    // units: seconds. If the object has never been seen, returns -1
    double timeSinceLastSeen() const;

    // units: seconds. If the object did not appear yet, returns -1
    double timeSinceLastAppeared() const;

    void setDisappearTimeout(double t) { timeout=t; }
    double getDisappearTimeout() const { return timeout; }

    Q_INVOKABLE void attachPoint(SAttachedPoint *p, float x, float y);

    static void installInEngine(QScriptEngine *engine);

    static void addModel(const char *name, SoftSnake *s, QGLWidget *context);
    static SModel *findModel(const QString &name);
    static void updateAll();
    static void resetAll(QGLWidget *context);
    STexture *getTexture() {
        return snake->snake->softMatcher->getSTexture();
    }
    STexture *setTexture(STexture *tex) { snake->snake->softMatcher->setSTexture(tex); return tex; }

    bool getEnableDetection() const { return snake->enableDetection; }
    bool setEnableDetection(bool d) { return snake->enableDetection=d; }
    bool getReshader() const { return snake->enableReshader; }
    bool setReshader(bool r) { return snake->enableReshader=r; }
    bool getDisplayFlag() const { return snake->snake->softMatcher->displayFlag; }
    bool setDisplayFlag(bool d) { return snake->snake->softMatcher->displayFlag = d; }

public slots:
    void update();
    void display();

public:
    SoftSnake *snake;
    QGLWidget *context;

private:
    QTime last_seen;
    QTime last_appeared;
    bool lost;
    double timeout;

    static std::map<QString, SModel *> models;
};

Q_DECLARE_METATYPE(SModel *)

#endif // SMODEL_H
