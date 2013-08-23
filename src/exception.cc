#include "couchbase_impl.h"
#include <sstream>

namespace Couchnode {

CBExc::CBExc(const char *msg, Handle<Value> at)
    : message(msg), minor_(ERR_GENERIC), major_(EXC_INTERNAL),
      err(LCB_SUCCESS), isSet(true)
{
    String::AsciiValue valstr(at);
    if (*valstr) {
        std::stringstream ss;
        ss << message;
        ss << "at '";
        ss << *valstr;
        ss << "'";
        message = ss.str();
    }
}

CBExc::CBExc()
    : message(""), minor_(ERR_OK), major_(EXC_SUCCESS),
      err(LCB_SUCCESS), isSet(false)
{
}

void CBExc::assign(ErrType etype, ErrCode ecode, const std::string &msg)
{
    if (isSet) {
        return;
    }
    major_ = etype;
    minor_ = ecode;
    isSet = true;
    message = msg;
}

void CBExc::assign(lcb_error_t lcberr)
{
    if (isSet) {
        return;
    }
    err = lcberr;
    assign(EXC_LCB, ERR_GENERIC, "libcouchbase Error");
}

std::string CBExc::formatMessage() const
{
    std::stringstream ss;
    ss << "[couchnode]: ";
    ss << "LCB=" << std::dec << err << ";";
    ss << "LCB_Expanded='"<< lcb_strerror(NULL, err) << "';";
    ss << "Major=" << std::dec << major_ << ";";
    ss << "Minor=" << std::dec << minor_ << ";";
    ss << "Message='" << message << "';";
    return ss.str();
}

Handle<Value> CBExc::asValue()
{
    Handle<Object> obj = Object::New();
    obj->Set(String::New("lcb_error"), Number::New(err));
    obj->Set(String::New("major"), Number::New(major_));
    obj->Set(String::New("minor"), Number::New(minor_));
    obj->Set(String::New("message"), String::New(message.c_str(),
                                                 message.length()));

    Handle<Array> arr = Array::New(2);
    arr->Set(0, String::New(formatMessage().c_str()));
    arr->Set(1, obj);
    return arr;
}

}
