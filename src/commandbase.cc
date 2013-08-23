#include "couchbase_impl.h"

namespace Couchnode {

using namespace v8;

bool Command::getBufBackedString(const Handle<Value> v, char **k, size_t *n)
{
    if (v.IsEmpty()) {
        return false;
    }

    Handle<String> s = v->ToString();

    if (s.IsEmpty()) {
        return false;
    }

    *n = s->Utf8Length();
    *k = bufs.getBuffer(*n);

    if (!*k) {
        err.eMemory("Couldn't get buffer");
    }

    int nw = s->WriteUtf8(*k, *n, NULL, String::NO_NULL_TERMINATION);
    if (nw < 0 || (unsigned int)nw < *n) {
        err.eInternal("Incomplete conversion");
        return false;
    }

    return true;
}

bool Command::initialize()
{
    keys = apiArgs[0];

    if (keys->IsArray()) {
        kcollType = ArrayKeys;
        ncmds = keys.As<Array>()->Length();

    } else if (keys->IsObject()) {
        kcollType = ObjectKeys;
        Local<Array> propNames(keys.As<Object>()->GetPropertyNames());
        ncmds = propNames->Length();
    } else {
        kcollType = SingleKey;
        ncmds = 1;
    }


    Parameters* params = getParams();
    Handle<Object> objParams(apiArgs[1].As<Object>());

    if (!initCommandList()) {
        err.eMemory("Command list");
        return false;
    }

    if (!objParams.IsEmpty()) {
        if (!params->parseObject(objParams, err)) {
            return false;
        }
    }

    if (!parseCommonOptions(objParams)) {
        return false;
    }

    return true;
}

bool Command::processSingle(Handle<Value> single,
                            Handle<Value> options,
                            unsigned int ix)
{
    char *k;
    size_t n;

    if (!getBufBackedString(single, &k, &n)) {
        return false;
    }

    return getHandler()(this, k, n, options, ix);
}

bool Command::processArray(Handle<Array> arry)
{
    Handle<Value> dummy;
    for (unsigned int ii = 0; ii < arry->Length(); ii++) {
        Handle<Value> cur = arry->Get(ii);
        if (!processSingle(cur, dummy, ii)) {
            return false;
        }
    }

    return true;
}

bool Command::processObject(Handle<Object> obj)
{


    Handle<Array> dKeys = obj->GetPropertyNames();
    for (unsigned int ii = 0; ii < dKeys->Length(); ii++) {
        Handle<Value> curKey = dKeys->Get(ii);
        Handle<Value> curValue = obj->Get(curKey);

        if (!processSingle(curKey, curValue, ii)) {
            return false;
        }
    }

    return true;
}

bool Command::process(ItemHandler handler)
{
    char *k;
    size_t nkey;

    switch (kcollType) {
    case SingleKey: {
        Handle<Value> v;
        return processSingle(keys, v, 0);
    }

    case ArrayKeys: {
        Handle<Array> arr = Handle<Array>::Cast(keys);
        return processArray(arr);
    }

    case ObjectKeys: {
        Handle<Object> obj = Handle<Object>::Cast(keys);
        return processObject(obj);
    }

    default:
        abort();
        break;


    }
    return false;
}

bool Command::parseCommonOptions(const Handle<Object> obj)
{
    if (!callback.parseValue(apiArgs[apiArgs.Length()-1], err)) {
        return false;
    }

    ParamSlot *spec = &isSpooled;
    if (!ParamSlot::parseAll(obj, &spec, 1, err)) {
        return false;
    }

    if (!callback.isFound()) {
        err.eArguments("Missing callback");
        return false;
    }

    return true;
}

Cookie* Command::createCookie()
{
    if (cookie) {
        return cookie;
    }


    cookie = new Cookie(ncmds);
    CallbackMode cbMode;
    if (isSpooled.isFound() && isSpooled.v) {
        cbMode = CBMODE_SPOOLED;
    } else {
        cbMode = CBMODE_SINGLE;
    }

    cookie->setCallback(callback.v, cbMode);
    return cookie;
}

Command::Command(Command &other)
    : apiArgs(other.apiArgs), cookie(other.cookie), bufs(other.bufs) {}

};
