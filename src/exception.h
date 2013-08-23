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
    EXC_LCB = 0x02,
    EXC_INTERNAL = 0x03
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
    CBExc(const char *, const Handle<Value>);
    CBExc();

    void assign(ErrType, ErrCode, const std::string & msg = "");
    void assign(lcb_error_t);


    // Handy method to assign an argument error
    // These all return the object so one can do:
    // CBExc().eArguments().throwV8();

    CBExc& eArguments(const std::string& msg = "") {
        assign(EXC_ARGUMENTS, ERR_USAGE, msg);
        return *this;
    }

    CBExc& eMemory(const std::string& msg = "") {
        assign(EXC_INTERNAL, ERR_MEMORY, msg);
        return *this;
    }

    CBExc& eInternal(const std::string &msg = "") {
        assign(EXC_INTERNAL, ERR_GENERIC, msg);
        return *this;
    }

    CBExc& eLcb(lcb_error_t err) {
        assign(err);
        return *this;
    }

    virtual const std::string &getMessage() const {
        return message;
    }

    virtual std::string formatMessage() const;

    Handle<Value> throwV8() { return throwV8Object(); }
    Handle<Value> asValue();


    Handle<Value> throwV8Object() {
        return v8::ThrowException(asValue());
    }

    Handle<Value> throwV8String() {
        return v8::ThrowException(String::New(formatMessage().c_str()));
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
