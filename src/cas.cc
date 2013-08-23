/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "couchbase_impl.h"
#include <sstream>
using namespace Couchnode;

/**
 * If we have 64 bit pointers we can stuff the pointer into the field and
 * save on having to make a new uint64_t*
 */
#if defined(_LP64) && !defined(COUCHNODE_NO_CASINTPTR)

Handle<Value> Cas::CreateCas(uint64_t cas)
{
    return External::New((void*)cas);
}

bool Cas::GetCas(Handle<Value> obj, uint64_t *p)
{
    if (!obj->IsExternal()) {
        return false;
    }
    *p = (uint64_t)External::Cast(*obj)->Value();
    return true;
}

#else

static void casDtor(Persistent<Value> obj, void *) {
    uint64_t *p = obj.As<Object>()->GetIndexedPropertiesExternalArrayData();
    delete p;
    obj.Dispose();
    obj.Clear();
}


inline Handle<Value> Cas::CreateCas(uint64_t cas) {
    Persistent<Object> ret = Persistent<Object>::New(Object::New());
    uint64_t *p = new uint64_t(cas);
    ret->SetIndexedPropertiesToExternalArrayData(p,
                                                 kExternalByteArray, sizeof(cas));
    ret.MakeWeak(NULL, casDtor);
    return ret;
}

inline bool Cas::GetCas(Handle<Value> obj, uint64_t *p) {
    Handle<Object> realObj = obj.As<Object>();
    if (!realObj->IsObject()) {
        return false;
    }
    if (realObj->GetIndexedPropertiesExternalArrayDataLength() != sizeof(*p)) {
        return false;
    }
    *p = *(uint64_t)realObj->GetIndexedPropertiesExternalArrayData();
    return true;
}
#endif
