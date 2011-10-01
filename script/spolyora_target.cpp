#include "spolyora_target.h"

#include <string.h>  // for memcpy

#include <QScriptEngine>
#include <QGLWidget>

using std::map;

QString SPolyoraTargetCollection::idToName(long id) {
    return QString("vobj%1").arg(id);
}

SPolyoraTarget* SPolyoraTargetCollection::getTarget(int id) {
    QString name = idToName(id);
    SPolyoraTarget* target = findChild<SPolyoraTarget*>(name);
    if (target != 0) {
	return target;
    }

    target = new SPolyoraTarget(this);
    target->setObjectName(name);
    return target;
}

void SPolyoraTargetCollection::processFrame(const vobj_frame* frame) {

    // All active objects disappear by default.
    for (map<long, SPolyoraTarget*>::iterator it(active_objects.begin());
	 it != active_objects.end(); ++it) {
	it->second->setInstance(0);
    }

    // (re)activates all targets visible in <frame>
    for (vobj_instance_vector::const_iterator
	 it(frame->visible_objects.begin()); it != frame->visible_objects.end();
	 ++it) {
	long id(it->object->id());
	SPolyoraTarget* target = findChild<SPolyoraTarget*>(idToName(id));
	if (target != 0) {
	    active_objects[id] = target;
	    target->setInstance(&*it);
	}
    }

    // Update all active objects.
    for (map<long, SPolyoraTarget*>::iterator it(active_objects.begin());
	 it != active_objects.end(); ) {
	if (!it->second->update()) {
	    // Target is now inactive.
	    active_objects.erase(it++);
	} else {
	    ++it;
	}
    }
}

namespace {

QScriptValue getTargetScript(
	QScriptContext *context, QScriptEngine *engine) {
    SPolyoraTargetCollection* _this = qobject_cast<SPolyoraTargetCollection*>(
	    context->thisObject().toQObject());
    if (_this == 0) {
	return context->throwError(QScriptContext::TypeError,
		"SPolyoraTargetCollection.getTarget: invalid type of 'this' "
		"object.");
    }

    if (context->argumentCount() < 1) {
	return context->throwError(QScriptContext::TypeError,
		"SPolyoraTargetCollection.getTarget takes an int as argument");
    }
    int id = context->argument(0).toNumber();
    SPolyoraTarget* target = _this->getTarget(id);
    if (target == 0) {
	return engine->undefinedValue();
    }
    return engine->newQObject(target, QScriptEngine::QtOwnership);
}

void setupHomography(const float H[3][3])
{
	float m[4][4];

	//glLoadIdentity();
	m[0][0]=H[0][0];  m[1][0]=H[0][1];  m[2][0]=0;  m[3][0]=H[0][2];
	m[0][1]=H[1][0];  m[1][1]=H[1][1];  m[2][1]=0;  m[3][1]=H[1][2];
	m[0][2]=0;        m[1][2]=0;        m[2][2]=1;  m[3][2]=0;
	m[0][3]=H[2][0];  m[1][3]=H[2][1];  m[2][3]=0;  m[3][3]=H[2][2];

	glMultMatrixf(&m[0][0]);
}

}  // namespace.

void SPolyoraTargetCollection::installInEngine(QScriptEngine* engine) {
    qRegisterMetaType<SPolyoraTarget *>();
    QScriptValue polyora = 
	    engine->newQObject(this, QScriptEngine::QtOwnership);
    engine->globalObject().setProperty("polyora", polyora);
    polyora.setProperty("getTarget", engine->newFunction(getTargetScript, 1));
}

SPolyoraTarget::SPolyoraTarget(QObject* parent)
    : QObject(parent), instance(0), lost(true), timeout(1) { }

bool SPolyoraTarget::update()
{
    if (isDetected()) {
        lost=false;
	memcpy(&last_good_homography, instance->transform,
		sizeof(last_good_homography));
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
	    return false;
        }
    }
    return true;
}

double SPolyoraTarget::timeSinceLastSeen() const
{
    if (last_seen.isNull()) return -1;
    return last_seen.elapsed() / 1000.0;
}

double SPolyoraTarget::timeSinceLastAppeared() const
{
    if (last_appeared.isNull()) return -1;
    return last_appeared.elapsed() /1000.0;
}

void SPolyoraTarget::pushTransform() {
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	if (!lost) {
	    setupHomography(last_good_homography);
	}
}

void SPolyoraTarget::popTransform() {
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}
