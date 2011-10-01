#include "stime.h"

Q_SCRIPT_DECLARE_QMETAOBJECT ( STime, QObject * )

QScriptValue stimeToScriptValue(QScriptEngine *engine, STime * const  &in)
{ return engine->newQObject(in); }

 void stimeFromScriptValue(const QScriptValue &object, STime * &out)
 { out = qobject_cast<STime *>(object.toQObject()); }


void STime::installInEngine(QScriptEngine *engine)
{
     QScriptValue Qt = engine->globalObject().property("Qt");
     if (Qt.isError()) {
         Qt = engine->globalObject();
     }
     QScriptValue qClass = engine->scriptValueFromQMetaObject<STime>();
     Qt.setProperty("Time", qClass);

     qScriptRegisterMetaType(engine, stimeToScriptValue, stimeFromScriptValue);

}
