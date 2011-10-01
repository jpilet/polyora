#include <assert.h>
#include <QFileInfo>
#include "stexture.h"

STexture::STexture(QGLWidget *context, GLuint tex, bool owner)
    : context(context), texture(tex), owner(owner),width(0), height(0)
{
    setDefaultCoord();
    playing=false;
    firstFrame=lastFrame=loopFromFrame=0;
    fps=15;
    loop=false;
    position=0;
    in_callback=false;
}

STexture::~STexture()
{
    release();
}

void STexture::release()
{
    if (owner && isValid())
        context->deleteTexture(texture);
    currentTexName.clear();
    texture = 0;
    width=height=0;
}

bool STexture::loadDDS(QString s)
{
    context->makeCurrent();
    GLuint t = context->bindTexture(s);
    //qDebug() << "loadDDS(" << s << "): " << t;

    if (t==0) {
        qDebug() << s << ": invalid texture file.";
        currentTexName = "";
    } else {
        currentTexName = s;
    }
    setTexture(t,-1,-1,false); // the cache is the texture owner
    return texture!=0;
}

bool STexture::clearAndLoadDDS(QString s)
{
    clearCallBacks();
    return loadDDS(s);
}

void STexture::setTexture(GLuint tex, int w, int h, bool owner)
{
    if (tex==texture) return;
    release();
    this->owner=owner;
    texture=tex;
    if (tex) {
        context->makeCurrent();
        glBindTexture(GL_TEXTURE_2D, tex);
        if (w>0)
            width=w;
        else
            glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        if (h>0)
            height=h;
        else
            glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    }
}

void STexture::setDefaultCoord()
{
    coords[0].move(0,0);
    coords[1].move(1,0);
    coords[2].move(1,1);
    coords[3].move(0,1);
}

void STexture::verticalFlip()
{
    coords[0].swap(coords[3]);
    coords[1].swap(coords[2]);
}

void STexture::horizontalFlip()
{
    coords[0].swap(coords[1]);
    coords[2].swap(coords[3]);
}

void STexture::setRatioCoord(const SPoint *a, const SPoint *b, const SPoint *c, const SPoint *d)
{
    coords[0].move(a->getX(), a->getY());
    coords[1].move(b->getX(), b->getY());
    coords[2].move(c->getX(), c->getY());
    coords[3].move(d->getX(), d->getY());
}
void STexture::setPixelCoord(const SPoint *a, const SPoint *b, const SPoint *c, const SPoint *d)
{
    float w = 1.0f/width;
    float h = 1.0f/height;
    coords[0].move(w*a->getX(), h*a->getY());
    coords[1].move(w*b->getX(), h*b->getY());
    coords[2].move(w*c->getX(), h*c->getY());
    coords[3].move(w*d->getX(), h*d->getY());
}

void STexture::bind()
{
    if (this==0) return;

    if (videoReady()) {
        QString s=frameFilename(updateCurrentFrame());
        loadDDS(s);
        //qDebug() << s << ": " << texture;
    } else if (currentTexName.size()>0)
        setTexture(context->bindTexture(currentTexName), width, height, false);

    if (texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
}

QScriptValue stextureToScriptValue(QScriptEngine *engine, STexture* const  &in)
{ return engine->newQObject(in); }

void stextureFromScriptValue(const QScriptValue &object, STexture * &out)
{
    out = qobject_cast<STexture *>(object.toQObject());
}

void STexture::installInEngine(QScriptEngine *engine, QGLWidget *context, unsigned long frameTexture)
{
    qScriptRegisterMetaType(engine, stextureToScriptValue, stextureFromScriptValue);
    STexture *frametex = new STexture(context, frameTexture, false);
    QScriptValue s = engine->newQObject(frametex, QScriptEngine::ScriptOwnership);
    engine->globalObject().setProperty("inputFrame", s, QScriptValue::ReadOnly);
}

// video stuff
QString STexture::frameFilename(unsigned frameno)
{
    QString r;
    r.sprintf(videoFileName.toAscii().data(), frameno);
    return r;
}

template <class T>
static T max(T a, T b)
{
    return (a>b ? a:b);
}

void STexture::clearCallBacks()
{
    disconnect(SIGNAL(onVideoStart()),0,0);
    disconnect(SIGNAL(onVideoStop()),0,0);
}


bool STexture::openVideo(QString s, double fps, bool loop, int start, int stop)
{
    clearCallBacks();
    setPlaying(false);
    setFPS(fps);
    setLooping(loop);

    videoFileName = s;

    bool ok=false;
    for (physicalFirstFrame=0; physicalFirstFrame < (unsigned)max(20,max(start,stop)); physicalFirstFrame++)
    {
        QString fn = frameFilename(physicalFirstFrame);
        QFileInfo info(fn);
        if (info.isFile() && info.isReadable()) {
            ok=true;
            break;   
        }
    }
    if (!ok) {
        videoFileName.clear();
        return false;
    }
    unsigned in = physicalFirstFrame;
    unsigned out = 100000;
    while (out-in > 1) {
        int mid = in + ((out-in)/2);
        QString fn = frameFilename(mid);
        QFileInfo info(fn);
        if (info.isFile() && info.isReadable())
            in = mid;
        else
            out = mid;
    }
    physicalLastFrame=in;
    position=firstFrame=physicalFirstFrame;
    lastFrame=physicalLastFrame;

    if (start>=0) setFirstFrame(start);
    if (stop>=0) setLastFrame(stop);
    return true;
}

bool STexture::setPlaying(bool _playing)
{
    // immediately return if there's nothing to do.
    if (_playing == isPlaying()) return _playing;
    if (!videoReady()) return false;

    if (isPlaying()) {
        // stop the video
        playing = false;
        if (!in_callback) {
            in_callback = true;
            emit onVideoStop();
            in_callback = false;
        }
    } else {
        // start the video
        playing = true;
        lastFrameTime.start();
        if (!in_callback) {
            in_callback=true;
            emit onVideoStart();
            in_callback=false;
        }
    }
    return isPlaying();
}

unsigned STexture::getCurrentFrame() const
{
    return position;
}

unsigned STexture::updateCurrentFrame()
{
    if (!videoReady()) return 0;

    if (isPlaying()) {
        double elapsed = lastFrameTime.restart()/1000.0;
        position += elapsed*fps;
    }

    if (loopFromFrame < firstFrame || loopFromFrame>= lastFrame)
        loopFromFrame = firstFrame;

    // this can happend if fps is negative.
    if (position < firstFrame) {
        if (loop) {
            position =  lastFrame-fmod(firstFrame-position, 1+lastFrame-firstFrame);
            assert(position >= firstFrame && position <= lastFrame);
        } else {
            position = firstFrame;
            setPlaying(false);
        }
    }
    if (position > lastFrame) {
        if (loop) {
            position = fmod(position-loopFromFrame, 1+lastFrame-loopFromFrame) + loopFromFrame;
            assert(position >= firstFrame && position <= lastFrame);
        } else {
            position = lastFrame;
            setPlaying(false);
        }
    }
    return getCurrentFrame();
}

unsigned STexture::setCurrentFrame(unsigned frameno)
{
    position = frameno;
    return updateCurrentFrame();
}

double STexture::getVideoTime() const
{
    return (position-firstFrame) / fps;
}

double STexture::setVideoTime(double sec)
{
    return setCurrentFrame(sec*fps+firstFrame) / fps;
}

unsigned STexture::setFirstFrame(unsigned frameno)
{
    if (frameno>=lastFrame) return firstFrame;
    if (frameno<physicalFirstFrame) frameno=physicalFirstFrame;
    firstFrame = frameno;
    return updateCurrentFrame();
}

unsigned STexture::setLoopFromFrame(unsigned frameno)
{
    unsigned n=frameno;
    if (n < firstFrame) n=firstFrame;
    if (n >= lastFrame) n=lastFrame;
    loopFromFrame = n;
}

unsigned STexture::setLastFrame(unsigned frameno)
{
    if (frameno<=firstFrame) return lastFrame;
    if (frameno>physicalLastFrame) frameno=physicalLastFrame;
    lastFrame = frameno;
    return updateCurrentFrame();
}
