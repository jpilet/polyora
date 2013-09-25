#ifndef STEXTURE_H
#define STEXTURE_H

#include <QObject>
#include <QScriptEngine>
#include <QGLWidget>
#include <QTime>
#include "spoint.h"

class STexture : public QObject
{
    Q_OBJECT;

    Q_PROPERTY(bool valid READ isValid);
    Q_PROPERTY(double width READ getWidth);
    Q_PROPERTY(double height READ getHeight);

    Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying);
    Q_PROPERTY(double fps READ getFPS WRITE setFPS);
    Q_PROPERTY(bool loop READ getLooping WRITE setLooping);
    Q_PROPERTY(unsigned firstFrame READ getFirstFrame WRITE setFirstFrame);
    Q_PROPERTY(unsigned lastFrame READ getLastFrame WRITE setLastFrame);
    Q_PROPERTY(unsigned loopFromFrame READ getLoopFromFrame WRITE setLoopFromFrame);
    Q_PROPERTY(double videoTime READ getVideoTime WRITE setVideoTime);
    Q_PROPERTY(unsigned currentFrame READ getCurrentFrame WRITE setCurrentFrame);
    Q_PROPERTY(bool videoReady READ videoReady);

signals:
    void onVideoStart();
    void onVideoStop();

public:
    STexture(QGLWidget *context, GLuint tex=0, bool owner=true);

    ~STexture();

    void bind();

    static void installInEngine(QScriptEngine *engine, QGLWidget *context, unsigned long frameTexture);

    void setTexture(GLuint tex, int w, int h, bool owner=true);

    double getWidth() const { return width; }
    double getHeight() const { return height; }

    // video properties accessors
    bool isPlaying() const { return playing; }
    bool setPlaying(bool playing);
    bool getLooping() const { return loop; }
    bool setLooping(bool l) { return loop=l; }
    double getFPS() const { return fps; }
    double setFPS(double _fps) { return fps=_fps; }
    unsigned getCurrentFrame() const;
    unsigned setCurrentFrame(unsigned frameno);
    unsigned getFirstFrame() const { return firstFrame; }
    unsigned setFirstFrame(unsigned frameno);
    unsigned getLastFrame() const { return lastFrame; }
    unsigned setLastFrame(unsigned frameno);
    unsigned getLoopFromFrame() const { return loopFromFrame; }
    unsigned setLoopFromFrame(unsigned frameno);
    double getVideoTime() const;
    double setVideoTime(double t);
    bool videoReady() const { return !videoFileName.isEmpty(); }
    bool isValid() const { return videoReady() || texture!=0; }

    unsigned updateCurrentFrame();

public slots:

    SPoint *getRatioCoord(unsigned n) { if (n<4) return &coords[n]; return 0; }
    void setDefaultCoord();
    void verticalFlip();
    void horizontalFlip();
    void setRatioCoord(const SPoint *a, const SPoint *b, const SPoint *c, const SPoint *d);
    void setPixelCoord(const SPoint *a, const SPoint *b, const SPoint *c, const SPoint *d);
    bool loadDDS(QString s);
    bool clearAndLoadDDS(QString s);
    void clearCallBacks();
    //bool loadVideoFrame(QString &s, int frameno);

    bool openVideo(QString s, double fps=15, bool loop=true, int start=-1, int stop=-1);
    void release();

protected:
    QGLWidget *context;
    GLuint texture;
    bool owner;
    GLfloat width, height;
    SPoint coords[4];

    // video stuff
    QString videoFileName;
    bool playing;
    unsigned firstFrame, lastFrame;
    unsigned loopFromFrame;
    unsigned physicalFirstFrame, physicalLastFrame;
    double fps;
    bool loop;
    QTime lastFrameTime;
    double position; // in number of frames

    QString frameFilename(unsigned frameno);

    QString currentTexName;

    bool in_callback;

    QImage image;
};

Q_DECLARE_METATYPE(STexture *)

#endif // STEXTURE_H
