#ifndef SPOLYORA_OBJ_H
#define SPOLYORA_OBJ_H

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QTime>

#include <map>

#include "../polyora/visual_database.h"
#include "../polyora/vobj_tracker.h"

class QScriptEngine;
class SPoint;
class SPolyoraTarget;

/*
The script conterpart of a visual_database. Takes care of sending
appearing/disappearing events to the script engine.

Basically, getTarget() instanciates SPolyoraTarget objects. The objects are
added as children of SPolyoraTargetCollection, with their name set to a string
representation of polyora's object id.

There is only one SPolyoraTarget per object id.
*/
class SPolyoraTargetCollection : public QObject {
    Q_OBJECT

public:
    SPolyoraTargetCollection(visual_database* db, QObject* parent)
	: QObject(parent), database(db) { }

    void installInEngine(QScriptEngine *engine);

    void processFrame(const vobj_frame* frame);

    static QString idToName(long obj_id);

    SPolyoraTarget* getTarget(int obj_id);

private:
    std::map<long, SPolyoraTarget*> active_objects;

    visual_database* database;
};

Q_DECLARE_METATYPE(SPolyoraTargetCollection *)

class SPolyoraHomography : public QObject {
    Q_OBJECT
public:
    SPolyoraHomography(QObject* parent) : QObject(parent) {
	H[0][0] = 1; H[0][1] = 0; H[0][2] = 0;
	H[1][0] = 0; H[1][1] = 1; H[1][2] = 0;
	H[2][0] = 0; H[2][1] = 0; H[2][2] = 1;
    }
    SPolyoraHomography(const SPolyoraHomography& a, QObject* parent) : QObject(parent) {
	copyFrom(a.H);
    }
    void copyFrom(const float h[3][3]) {
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			H[i][j] = h[i][j];
		}
	}
    }	
    void saveTo(float dst[3][3]) const {
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			dst[i][j] = H[i][j];
		}
	}
    }

public slots:
    void pushTransform();
    void popTransform();
    bool transformPoint(const SPoint* src, SPoint* dst);
    bool inverseTransformPoint(const SPoint* src, SPoint* dst);

private:
    float H[3][3];
};
Q_DECLARE_METATYPE(SPolyoraHomography *)

class SPolyoraTarget : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool lost READ isLost)
    Q_PROPERTY(bool detected READ isDetected)
    Q_PROPERTY(double sinceLastSeen READ timeSinceLastSeen)
    Q_PROPERTY(double sinceLastAppeared READ timeSinceLastAppeared)
    Q_PROPERTY(double disappearTimeout READ getDisappearTimeout WRITE setDisappearTimeout)
    Q_PROPERTY(double timeout READ getDisappearTimeout WRITE setDisappearTimeout)

signals:
    void moved();
    void appeared(); // a move signal will always follow an appeared signal.
    void disappeared();

public:
    SPolyoraTarget(QObject* parent);

    bool isDetected() const { return instance != 0; }
    bool isLost() const { return lost; }

    // Transmit the tracking result to the SPolyoraTarget object.
    // If <instance> is null, tracking failed.
    void setInstance(const vobj_instance* instance, const vobj_frame* frame) {
	this->instance = instance;
	this->frame = frame;
    }

    // Returns true if the object wants to be informed about next frame result.
    // Returns false if disappeared() has been sent, and this object is not
    // interested in further null instance notifications (becomes inactive).
    bool update();

public slots:
    void pushTransform() { 
	if (!lost) { last_good_homography->pushTransform(); }
    }
    void popTransform() {
	if (!lost) { last_good_homography->popTransform(); }
    }
    bool transformPoint(const SPoint* src, SPoint* dst) {
	if (!isDetected()) return false;
	return last_good_homography->transformPoint(src, dst);
    }
    bool inverseTransformPoint(const SPoint* src, SPoint* dst) {
	if (!isDetected()) return false;
	return last_good_homography->inverseTransformPoint(src, dst);
    }

    bool getSpeed(float x, float y, SPoint* dst);

    SPolyoraHomography* homography() { return new SPolyoraHomography(last_good_homography, 0); }
private:
    // units: seconds. If the object has never been seen, returns -1
    double timeSinceLastSeen() const;

    // units: seconds. If the object did not appear yet, returns -1
    double timeSinceLastAppeared() const;

    void setDisappearTimeout(double t) { timeout=t; }
    double getDisappearTimeout() const { return timeout; }

    const vobj_instance* instance;
    const vobj_frame* frame;
    QTime last_seen;
    QTime last_appeared;
    bool lost;
    double timeout;
    SPolyoraHomography* last_good_homography;
};

Q_DECLARE_METATYPE(SPolyoraTarget *)

#endif // SPOLYORA_OBJ_H
