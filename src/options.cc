#include "couchbase_impl.h"
#include <cstdlib>
namespace Couchnode {
using std::cerr;

bool ParamSlot::maybeSetFalse(Handle<Value> v)
{
    if (v->IsUndefined() || v->IsNull() || v->IsFalse()) {
        status = PARSE_OPTION_FALSEVAL;
        return true;
    }
    return false;
}

bool ParamSlot::parseAll(const Handle<Object> dict, ParamSlot **specs,
                         size_t nspecs, CBExc &ex)
{
    if (dict.IsEmpty()) {
        return true; // no options
    }

    if (!dict->IsObject()) {
        if (dict->BooleanValue()) {
            ex.eArguments("Value passed is not an object", dict);
            return false;
        }
        return true;
    }

    if (!dict->GetPropertyNames()->Length()) {
        return true;
    }

    for (unsigned int ii = 0; ii < nspecs; ii++ ) {
        ParamSlot *cur = specs[ii];
        Handle<String> name = cur->getName();
        Handle<Value> val = dict->Get(name);

        if (val.IsEmpty()) {
            continue;
        }

        ParseStatus status = cur->parseValue(val, ex);
        if (status == PARSE_OPTION_ERROR) {
            assert(ex.isSet());
            return false;
        }
    }
    return true;
}


ParseStatus CasSlot::parseValue(const Handle<Value> value, CBExc &ex)
{

    if (maybeSetFalse(value)) {
        return status;
    }

    if (!Cas::GetCas(value, &v)) {
        ex.eArguments("Bad CAS", value);
        return PARSE_OPTION_ERROR;
    }
    return PARSE_OPTION_FOUND;
}

ParseStatus CallableOption::parseValue(const Handle<Value> val, CBExc &ex)
{
    if (maybeSetFalse(val)) {
        return status;
    }

    if (!val->IsFunction()) {
        ex.eArguments("Expected callback", val);
        return returnStatus(PARSE_OPTION_ERROR);
    }

    v = Handle<Function>::Cast(val);
    return returnStatus(PARSE_OPTION_FOUND);
}

ParseStatus StringOption::parseValue(const Handle<Value> val, CBExc &ex)
{
    if (val->IsNumber() == false && val->IsString() == false) {
        ex.eArguments("String option must be number or string", val);
        return returnStatus(PARSE_OPTION_ERROR);
    }

    v = val->ToString();
    return returnStatus(PARSE_OPTION_FOUND);
}

};
