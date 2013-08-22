#ifndef COUCHBASE_EXCEPTION_H
#define COUCHBASE_EXCEPTION_H
#ifndef COUCHBASE_H
#error "include couchbase_impl.h first"
#endif

#include "namespace_includes.h"
namespace Couchnode {
/**
 * Base class of the Exceptions thrown by the internals of
 * Couchnode
 */

enum ErrType {
    EXC_SUCCESS = 0x00,
    EXC_ARGUMENTS = 0x01,
    EXC_LCB = 0x01,
    EXC_INTERNAL = 0x02
};

enum ErrCode {
    ERR_OK = 0x00,
    ERR_ARGS = 0x01,
    ERR_SCHED = 0x02,
    ERR_USAGE = 0x03,
    ERR_TYPE = 0x04,
    ERR_MEMORY = 0x05,
    ERR_GENERIC = 0x06,
};

class CBExc
{
public:
    CBExc(const char *msg, const Handle<Value> &at)
        : message(msg), minor_(ERR_OK), major_(EXC_SUCCESS), err(LCB_SUCCESS), isSet(true) {
        String::AsciiValue valstr(at);
        if (*valstr) {
            message += " at '";
            message += *valstr;
            message += "'";
        }
    }

    CBExc() :
        message(""), minor_(ERR_OK), major_(EXC_SUCCESS), err(LCB_SUCCESS), isSet(false) {}

    virtual ~CBExc() {
        // Empty
    }

    void assign(ErrType type, ErrCode code, const std::string &msg = "") {
        if (isSet) {
            return;
        }
        major_ = type;
        minor_ = code;
        isSet = true;
    }

    // Handy method to assign an argument error
    void eArguments(const std::string& msg = "") {
        assign(EXC_ARGUMENTS, ERR_USAGE, msg);
    }

    void eMemory(const std::string& msg = "") {
        assign(EXC_INTERNAL, ERR_MEMORY, msg);
    }

    void eInternal(const std::string &msg = "") {
        assign(EXC_INTERNAL, ERR_GENERIC, msg);
    }

    void eLcb(lcb_error_t err) {
        assign(err);
    }

    void assign(lcb_error_t lcberr, ErrCode detail = ERR_GENERIC) {
        if (isSet) {
            return;
        }

        major_ = EXC_LCB;
        minor_ = detail;
        err = lcberr;
        isSet = true;
    }

    virtual const std::string &getMessage() const {
        return message;
    }

    Handle<Value> throwV8() {
        Handle<Value> exObj = asObject();
        return v8::ThrowException(exObj);
    }

    Handle<Object> asObject() {
        Handle<Object> obj = Object::New();
        obj->Set(String::New("lcb_error"), Number::New(err));
        obj->Set(String::New("major"), Number::New(major_));
        obj->Set(String::New("minor"), Number::New(minor_));
        obj->Set(String::New("message"), String::New(message.c_str()));
        return obj;
    }

protected:
    std::string message;
    ErrCode minor_;
    ErrType major_;
    lcb_error_t err;
    bool isSet;
};

}

#endif
