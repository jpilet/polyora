#include "spolyora_target.h"

#include <string.h>  // for memcpy

#include <QScriptEngine>
#include <QGLWidget>

#include "homography.h"
#include "spoint.h"
#include "sattachedpoint.h"

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
	it->second->setInstance(0, 0);
    }

    // (re)activates all targets visible in <frame>
    for (vobj_instance_vector::const_iterator
	 it(frame->visible_objects.begin()); it != frame->visible_objects.end();
	 ++it) {
	long id(it->object->id());
	SPolyoraTarget* target = findChild<SPolyoraTarget*>(idToName(id));
	if (target != 0) {
	    active_objects[id] = target;
	    target->setInstance(&*it, frame);
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

QScriptValue attachScript(
	QScriptContext *context, QScriptEngine *engine) {
    SPolyoraTarget* _this = qobject_cast<SPolyoraTarget*>(
	    context->thisObject().toQObject());
    if (_this == 0) {
	return context->throwError(QScriptContext::TypeError,
		"SPolyoraTarget.attach: invalid type of 'this' "
		"object.");
    }

    if (context->argumentCount() < 1) {
	return context->throwError(QScriptContext::TypeError,
		"SPolyoraTarget.attach takes a 2d point as argument");
    }
    float x, y;
    if (context->argumentCount() == 1) {
	SPoint *a = qobject_cast<SPoint *>(context->argument(0).toQObject());
	if (!a) {
	    return context->throwError(QScriptContext::TypeError,
		    "SPolyoraTarget.attach takes a 2d point as argument");
	}
	x = a->getX();
	y = a->getY();
    } else {
	x = (float)context->argument(0).toNumber();
	y = (float)context->argument(1).toNumber();
    }
    return engine->newQObject(new SAttachedPoint(_this, x, y),
	    QScriptEngine::ScriptOwnership);
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


template<class T>
QScriptValue polyoraXToScriptValue(QScriptEngine *engine, T* const  &in)
{ return engine->newQObject(in); }

template<class T>
void polyoraXFromScriptValue(const QScriptValue &object, T* &out)
 { out = qobject_cast<T *>(object.toQObject()); }

}  // namespace.

void SPolyoraTargetCollection::installInEngine(QScriptEngine* engine) {
    QScriptValue prototype = engine->newObject();
    prototype.setProperty("attach", engine->newFunction(attachScript, 2));
    qScriptRegisterMetaType(engine, polyoraXToScriptValue<SPolyoraTarget>,
	    polyoraXFromScriptValue<SPolyoraTarget>, prototype);
    prototype = engine->newObject();
    qScriptRegisterMetaType(engine, polyoraXToScriptValue<SPolyoraHomography>,
	    polyoraXFromScriptValue<SPolyoraHomography>, prototype);

    QScriptValue polyora = 
	    engine->newQObject(this, QScriptEngine::QtOwnership);
    engine->globalObject().setProperty("polyora", polyora);
    polyora.setProperty("getTarget", engine->newFunction(getTargetScript, 1));
}

SPolyoraTarget::SPolyoraTarget(QObject* parent)
    : QObject(parent), instance(0), lost(true), timeout(1),
    last_good_homography(new SPolyoraHomography(parent)) { }

bool SPolyoraTarget::update()
{
    if (isDetected()) {
	last_good_homography->copyFrom(instance->transform);
        if (last_seen.isNull()) {
            last_seen.start();
            last_appeared.start();
	    lost=false;
            emit appeared();
        } else if (last_seen.restart() > (int)(timeout*1000.0)) {
            last_appeared.restart();
	    lost=false;
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

void SPolyoraHomography::pushTransform() {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    setupHomography(H);
}

void SPolyoraHomography::popTransform() {
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

bool SPolyoraHomography::transformPoint(const SPoint* src, SPoint* dst) {
    if (!src || !dst) {
      return false;
    }
    const float input[] = { src->getX(), src->getY() };
    float transformed[2];
    script_homography::homography_transform(input, H, transformed);
    dst->move(transformed[0], transformed[1]);
    return true;
}

bool SPolyoraHomography::inverseTransformPoint(const SPoint* src, SPoint* dst) {
    if (!src || !dst) {
	return false;
    }
    const float input[] = { src->getX(), src->getY() };
    float transformed[2];
    float inverse[3][3];
    // TODO: cache the inversion.
    script_homography::homography_inverse(H, inverse);
    script_homography::homography_transform(input, inverse, transformed);
    dst->move(transformed[0], transformed[1]);
	return true;
}

bool SPolyoraTarget::getSpeed(float x, float y, SPoint* dst) {
    if (!isDetected()) {
	return false;
    }

    // scan at most 10 frames in the past to search for a previous position.
    const vobj_instance* prev_instance = 0; 
    const vobj_frame* prev_frame = frame; 
    for (int i = 0; i < 10; i++) {
	prev_frame = static_cast<const vobj_frame*>(prev_frame->frames.next);
	if (!prev_frame) {
	    return false;
	}
	prev_instance = prev_frame->find_instance(instance->object);
	if (prev_instance) {
	    break;
	}
    }
    if (!prev_instance) {
	return false;
    }

    const float input[] = {x, y};
    float transformed[2];
    script_homography::homography_transform(input, instance->transform, transformed);
    float prev_transformed[2];
    script_homography::homography_transform(input, prev_instance->transform, prev_transformed);
    float time_delta = (frame->timestamp - prev_frame->timestamp) / 1000.0;
    dst->move((transformed[0] - prev_transformed[0]) / time_delta,
	      (transformed[1] - prev_transformed[1]) / time_delta);
    return true;
}
