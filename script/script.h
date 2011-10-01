#ifndef SCRIPT_H
#define SCRIPT_H

#include <QObject>
#include <QScriptEngine>

#include <polyora/visual_database.h>
#include <polyora/vobj_tracker.h>


class QGLWidget;
class QApplication;
class SPolyoraTargetCollection;
class SGraphics;

class Script : public QObject
{
    Q_OBJECT
public:
    explicit Script(QObject *parent = 0);

    void setup(QGLWidget *context, visual_database* db, QApplication* app,
	       unsigned long frame_texture);

    bool isReady() const { return setup_done; }

signals:

public slots:
    bool runScript(const QString& scriptFileName);
    void processFrame(const vobj_frame* frame);

    // Returns false if the script failed. It's a good idea to pause execution.
    bool render();

private:
    visual_database* database;
    QGLWidget* gl_context;
    QScriptEngine engine;
    SGraphics* sgraphics;
    SPolyoraTargetCollection* polyora_targets;
    bool ready, setup_done;
};

#endif // SCRIPT_H
