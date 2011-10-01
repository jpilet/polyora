#include "script.h"

#include <QApplication>
#include <QMetaObject>
#include <QtScript>
#include <QFileInfo>
#include <QDir>

#include "spoint.h"
#include "stexture.h"
#include "stime.h"
#include "sgraphics.h"
#include "spolyora_target.h"


namespace {

bool execScript(const QString& name, QScriptEngine *engine)
{
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << name << ": unable to open script file.";
        return false;
    }

    QString contents = file.readAll();
    file.close();
    QScriptValue r = engine->evaluate(contents, name);
    if (r.isError()) {
        qDebug() << "Script error: " << r.toString();
    }
    if (engine->hasUncaughtException()) {
        QString eol("\n");
        qDebug() << engine->uncaughtExceptionBacktrace().join(eol);
    }
    return true;
}

QScriptValue includeScript(QScriptContext *context, QScriptEngine *engine)
{
    for (int i=0; i<context->argumentCount(); i++) {
        QString name = context->argument(i).toString();
        if (!execScript(name, engine)) {
            return context->throwError(QString("can't include '%1'.").arg(name));
        }
    }
    return engine->undefinedValue();
}

}  // namespace

struct QtMetaObject : private QObject
{
public:
    static const QMetaObject *get()
    { return &static_cast<QtMetaObject*>(0)->staticQtMetaObject; }
};
Q_SCRIPT_DECLARE_QMETAOBJECT(QTimer, QObject*)


Script::Script(QObject *parent) :
    QObject(parent), gl_context(0), database(0), sgraphics(0),
    polyora_targets(0), setup_done(false), ready(false)
{
}

void Script::setup(QGLWidget *gl_context, visual_database* db, QApplication* app,
		   unsigned long frame_texture) {
    this->gl_context = gl_context;
    this->database = db;

    QScriptValue Qt = engine.newQMetaObject(QtMetaObject::get());
    if (app) {
	Qt.setProperty("application", engine.newQObject(app));
    }
    Qt.setProperty("window", engine.newQObject(gl_context, QScriptEngine::QtOwnership));
    engine.globalObject().setProperty("Qt", Qt);
    QScriptValue qtimerClass = engine.scriptValueFromQMetaObject<QTimer>();
    Qt.setProperty("Timer", qtimerClass);

    engine.globalObject().setProperty("include", engine.newFunction(includeScript,1));

    STime::installInEngine(&engine);
    SPoint::installInEngine(&engine);
    //SAttachedPoint::installInEngine(&engine);
    //SModel::installInEngine(&engine);
    STexture::installInEngine(&engine, gl_context, frame_texture);
    engine.pushContext();    

    setup_done = true;
}

bool Script::runScript(const QString& scriptFileName)
{
    assert(setup_done);

    ready = false;
    engine.popContext();
    //SModel::resetAll(this);
    engine.collectGarbage();
    engine.pushContext();

    if (sgraphics) delete sgraphics;
    sgraphics = new SGraphics(gl_context, this);
    sgraphics->installInEngine(&engine);

    if (polyora_targets) delete polyora_targets;
    polyora_targets = new SPolyoraTargetCollection(database, this);
    polyora_targets->installInEngine(&engine);

    if (!execScript(scriptFileName, &engine)) {
	return ready;
    }
    // clear uncaught exceptions
    engine.evaluate("");

    ready = true;
    return ready;
}

void Script::processFrame(const vobj_frame* frame) {
    assert(setup_done);
    if (!ready) return;
    if (!frame) return;
    polyora_targets->processFrame(frame);
}

bool Script::render() {
    assert(setup_done);
    if (!ready) {
	// Nothing to do. This is not an error.
	return true;
    }
    assert(sgraphics != 0);
    sgraphics->update();
    if (engine.hasUncaughtException()) {
	QString eol("ERROR:\n");
	qDebug() << engine.uncaughtException().toString();
	qDebug() << engine.uncaughtExceptionBacktrace().join(eol);
	return false;
    }
    return true;
}

