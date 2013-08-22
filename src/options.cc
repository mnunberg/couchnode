#include "couchbase_impl.h"
#include <cstdlib>
namespace Couchnode {
using std::cerr;
static inline bool isFalseValue(const Handle<Value> &v)
{
    return (v.IsEmpty() || v->BooleanValue() == false);
}

bool ParamSlot::parseAll(const Handle<Object>& dict,
                             ParamSlot **specs,
                             size_t nspecs,
                             CBExc &ex)
{
    if (dict.IsEmpty()) {
        return true; // no options
    }
    if (!dict->IsObject()) {
        if (dict->BooleanValue()) {
            ex.eArguments("Value passed is not an object");
            return false;
        }
        return true;
    }

    for (unsigned int ii = 0; ii < nspecs; ii++ ) {
        ParamSlot *cur = specs[ii];
        Handle<String> name = cur->getName();
        Handle<Value> val = dict->Get(name);

        if (val.IsEmpty()) {
            continue;
        }
        if (!dict->Has(name)) {
            continue;
        }

        ParseStatus status = cur->parseValue(val, ex);
        if (status == PARSE_OPTION_ERROR) {
            return false;
        }
    }
    return true;
}

ParseStatus ParamSlot::validateNumber(const Handle<Value> &val, CBExc &ex)
{
    if (! (val->IsNumber() || val->IsString())) {
        if (!isFalseValue(val)) {
            ex.eArguments("Non-false non-numeric value specified");
            return PARSE_OPTION_ERROR;
        }
        return PARSE_OPTION_EMPTY;
    }
    return PARSE_OPTION_FOUND;
}

ParseStatus CasSlot::parseValue(const Handle<Value>& value,
                                  CBExc &ex)
{
    if (isFalseValue(value)) {
        v = 0;
        return PARSE_OPTION_EMPTY;
    } else if (value->IsObject()) {
        v = Cas::GetCas(value->ToObject());
        return PARSE_OPTION_FOUND;

    } else {
        ex.eArguments("Invalid CAS Specified");
        return PARSE_OPTION_ERROR;
    }
    return PARSE_OPTION_ERROR;
}

ParseStatus ExpOption::parseValue(const Handle<Value> &value,
                                  CBExc &ex)
{
    ParseStatus status = validateNumber(value, ex);
    if (status != PARSE_OPTION_FOUND) {
        return status;
    }

    int32_t iv = value->IntegerValue();
    if (iv < 0) {
        ex.eArguments("Expiry cannot be negative");
        return PARSE_OPTION_ERROR;
    }

    v = iv;
    return PARSE_OPTION_FOUND;
}

ParseStatus FlagsOption::parseValue(const Handle<Value>& val, CBExc &ex)
{
    if ( (status = validateNumber(val, ex)) != PARSE_OPTION_FOUND) {
        return status;
    }

    v = val->Uint32Value();
    return returnStatus(PARSE_OPTION_FOUND);
}

ParseStatus CallableOption::parseValue(const Handle<Value> &val, CBExc &ex)
{
    if (isFalseValue(val)) {
        return returnStatus(PARSE_OPTION_EMPTY);
    }
    if (!val->IsFunction()) {
        ex.eArguments("Expected callback or false value");
        return returnStatus(PARSE_OPTION_ERROR);
    }

    v = Handle<Function>::Cast(val);
    return returnStatus(PARSE_OPTION_FOUND);
}

ParseStatus StringOption::parseValue(const Handle<Value> &val, CBExc &ex)
{
    if (val->IsNumber() == false && val->IsString() == false) {
        ex.eArguments("String option must be number or string");
        return returnStatus(PARSE_OPTION_ERROR);
    }

    v = val->ToString();
    return returnStatus(PARSE_OPTION_FOUND);
}

ParseStatus Uint64Option::parseValue(const Handle<Value> &val, CBExc &ex)
{
    if ( (status = validateNumber(val, ex)) != PARSE_OPTION_FOUND) {
        return status;
    }

    v = val->IntegerValue();
    return returnStatus(PARSE_OPTION_FOUND);
}

ParseStatus Int64Option::parseValue(const Handle<Value> &val, CBExc &ex)
{
    if ( (status = validateNumber(val, ex)) != PARSE_OPTION_FOUND) {
        return status;
    }

    v = val->IntegerValue();
    return status;
}

};
