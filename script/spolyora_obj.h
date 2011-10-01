#ifndef SPOLYORA_OBJ_H
#define SPOLYORA_OBJ_H

#include <QObject>
#include <../polyora/visual_database.h>
#include <../polyora/vobj_tracker.h>

class SPolyoraObj : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool detected READ isDetected)
    Q_PROPERTY(double sinceLastSeen READ timeSinceLastSeen)
    Q_PROPERTY(double sinceLastAppeared READ timeSinceLastAppeared)
    Q_PROPERTY(double disappearTimeout READ getDisappearTimeout WRITE setDisappearTimeout)

signals:
    void moved();
    void appeared(); // a move signal will always follow an appeared signal.
    void disappeared();

public:
    SPolyoraObj(int obj_id);

    bool isDetected() const;
    void update();

    // Transmit the tracking result to the SPolyoraObj object.
    // If <instance> is null, tracking failed.
    // Returns true if the object wants to be informed about next frame result.
    // Returns false if disappeared() has been sent, and this object is not
    // interested in further null instance notifications.
    bool setInstance(vobj_instance* instance);

private:
    // units: seconds. If the object has never been seen, returns -1
    double timeSinceLastSeen() const;

    // units: seconds. If the object did not appear yet, returns -1
    double timeSinceLastAppeared() const;

    void setDisappearTimeout(double t) { timeout=t; }
    double getDisappearTimeout() const { return timeout; }

    vobj_instance* instance;
    QTime last_seen;
    QTime last_appeared;
    bool lost;
    double timeout;
};

/*
The script conterpart of a visual_database. Takes care of sending
appearing/disappearing events to the script engine.

Basically, getTarget() instanciates SPolyoraObj objects. The objects are added
as children of SPolyoraObjCollection, with their name set to a string
representation of polyora's object id.

There is only one SPolyoraObj per object id.
*/
class SPolyoraObjCollection : public QObject {
    Q_OBJECT

public:
    SPolyoraObjCollection(visual_database* db);

    void installInEngine(QScriptEngine *engine);

    void processFrame(const vobj_frame* frame);

    static QString idToName(long obj_id);

public slots:
    SPolyoraObj* getTarget(long obj_id);

private:
    std::map<long, SPolyoraObj*> active_objects;
};

#endif SPOLYORA_OBJ_H
