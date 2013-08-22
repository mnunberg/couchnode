/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef COUCHNODE_COOKIE_H
#define COUCHNODE_COOKIE_H 1

#ifndef COUCHBASE_H
#error "Include couchbase.h before including this file"
#endif

namespace Couchnode
{
using namespace v8;

typedef enum {
    CBMODE_SINGLE,
    CBMODE_SPOOLED
} CallbackMode;

class ResponseInfo {
public:
    lcb_error_t status;
    Persistent<Object> payload;

    Handle<Value> getKey() {
        return String::New((const char *)key, nkey);
    }

    ~ResponseInfo() {
        payload.Dispose();
        payload.Clear();
    }

    ResponseInfo(lcb_error_t, const lcb_get_resp_t*);
    ResponseInfo(lcb_error_t, const lcb_store_resp_t *);
    ResponseInfo(lcb_error_t, const lcb_arithmetic_resp_t*);
    ResponseInfo(lcb_error_t, const lcb_touch_resp_t*);
    ResponseInfo(lcb_error_t, const lcb_unlock_resp_t *);
    ResponseInfo(lcb_error_t, const lcb_durability_resp_t *);
    ResponseInfo(lcb_error_t, const lcb_remove_resp_t *);
    ResponseInfo(lcb_error_t, const lcb_http_resp_t *resp);
    ResponseInfo(lcb_error_t);

    const void *key;
    size_t nkey;

private:
    ResponseInfo(ResponseInfo&);

    // Helpers
    void setCas(lcb_cas_t cas) {
        payload->Set(NameMap::names[NameMap::CAS], Cas::CreateCas(cas));
    }

    void setValue(Handle<Value>& val) {
        payload->Set(NameMap::names[NameMap::VALUE], val);
    }

};

class Cookie
{
public:
    Cookie(unsigned int numRemaining)
        : remaining(numRemaining), cbType(CBMODE_SINGLE), hasError(false) {}

    void setCallback(Handle<Function> cb, CallbackMode mode) {
        assert(callback.IsEmpty());
        callback = Persistent<Function>::New(cb);
        cbType = mode;
    }

    void setParent(v8::Handle<v8::Value> cbo) {
        assert(parent.IsEmpty());
        parent = v8::Persistent<v8::Value>::New(cbo);
    }

    virtual ~Cookie();
    void markProgress(ResponseInfo&);
    void cancel(lcb_error_t err);

protected:
    Persistent<Object> spooledInfo;
    void invokeFinal();

private:
    unsigned int remaining;
    void addSpooledInfo(Handle<Value>&, ResponseInfo&);
    void invokeSingleCallback(Handle<Value>&, ResponseInfo&);
    void invokeSpooledCallback();
    Persistent<Value> parent;
    Persistent<Function> callback;
    CallbackMode cbType;
    bool hasError;

    // No copying
    Cookie(Cookie&);
};

} // namespace Couchnode
#endif // COUCHNODE_COOKIE_H
