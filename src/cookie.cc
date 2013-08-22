/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "couchbase_impl.h"
#include <cstdio>
#include <sstream>

using namespace Couchnode;

Cookie::~Cookie()
{

    if (!parent.IsEmpty()) {
        parent.Dispose();
        parent.Clear();
    }

    if (!callback.IsEmpty()) {
        callback.Dispose();
        callback.Clear();
    }

    if (!spooledInfo.IsEmpty()) {
        spooledInfo.Dispose();
        spooledInfo.Clear();
    }
}

void Cookie::addSpooledInfo(Handle<Value>& ec, ResponseInfo& info)
{
    if (spooledInfo.IsEmpty()) {
        spooledInfo = Persistent<Object>::New(Object::New());
    }

    Handle<Array> arr = Array::New(2);
    arr->Set(0, ec);
    arr->Set(1, info.payload);
    Handle<Value> key = info.getKey();
    spooledInfo->Set(key, info.payload);
}

void Cookie::invokeSingleCallback(Handle<Value>& errNum,
                                           ResponseInfo& info)
{
    Handle<Value> args[2] = { errNum, info.payload };
    TryCatch try_catch;
    callback->Call(v8::Context::GetEntered()->Global(), 2, args);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

void Cookie::invokeSpooledCallback()
{
    Handle<Value> args[2] = { hasError ? v8::True() : v8::False(), spooledInfo };
    TryCatch try_catch;
    callback->Call(v8::Context::GetEntered()->Global(), 2, args);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}


void Cookie::markProgress(ResponseInfo &info) {
    remaining--;
    HandleScope scope;
    Handle<Value> errNum;

    if (info.status != LCB_SUCCESS) {
        hasError = true;
        errNum = Local<Number>(Number::New(info.status));
        if (info.nkey) {
            printf("Key %.*s failed (e=%d)\n", (int)info.nkey, info.key, info.status);
            abort();
        }
    } else {
        errNum = v8::Undefined();
    }

    if (cbType == CBMODE_SINGLE) {
        invokeSingleCallback(errNum, info);
    } else {
        addSpooledInfo(errNum, info);
    }

    if (remaining == 0 && cbType == CBMODE_SPOOLED) {
        invokeSpooledCallback();
    }

    if (!remaining) {
        delete this;
    }
}

void Cookie::cancel(lcb_error_t err)
{
    ResponseInfo ri(err);
    // Invoke this only while there is still a "remaining" count
    while (remaining > 1) {
        markProgress(ri);
    }

    assert(remaining == 1);
    markProgress(ri);
    // DELETED
}

template <typename T>
void initCommonInfo_v0(ResponseInfo *tp, lcb_error_t err, const T* resp)
{
    tp->key = resp->v.v0.key;
    tp->nkey = resp->v.v0.nkey;
    tp->status = err;
    tp->payload = Persistent<Object>::New(Object::New());
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_get_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
    if (err != LCB_SUCCESS) {
        return;
    }

    setCas(resp->v.v0.cas);
    payload->Set(NameMap::names[NameMap::FLAGS], Number::New(resp->v.v0.flags));

    // Get the value
    // @todo - make this JSON
    Handle<Value> s = String::New((const char*)resp->v.v0.bytes, resp->v.v0.nbytes);
    setValue(s);
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_store_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
    if (err != LCB_SUCCESS) {
        return;
    }
    setCas(resp->v.v0.cas);
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_arithmetic_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
    if (err != LCB_SUCCESS) {
        return;
    }

    setCas(resp->v.v0.cas);
    Handle<Value> num = Number::New(resp->v.v0.value);
    setValue(num);
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_touch_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
    if (err != LCB_SUCCESS) {
        return;
    }
    setCas(resp->v.v0.cas);
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_unlock_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_durability_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
    if (err != LCB_SUCCESS) {
        return;
    }

    if (resp->v.v0.err != LCB_SUCCESS) {
        status = resp->v.v0.err;
    }

    setCas(resp->v.v0.cas);
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_remove_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
    if (err != LCB_SUCCESS) {
        return;
    }
    setCas(resp->v.v0.cas);
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_http_resp_t *resp)
{
    status = err;
    if (resp->v.v0.nbytes) {
        Handle<Value> s = String::New((const char *)resp->v.v0.bytes,
                                      resp->v.v0.nbytes);
        setValue(s);
    }

    payload->Set(NameMap::names[NameMap::HTCODE], Number::New(resp->v.v0.status));

}

ResponseInfo::ResponseInfo(lcb_error_t err) : key(NULL), nkey(0)
{
    status = err;
}



static inline Cookie *getInstance(const void *c)
{
    return reinterpret_cast<Cookie *>(const_cast<void *>(c));
}

// @todo we need to do this a better way in the future!
static void unknownLibcouchbaseType(const std::string &type, int version)
{
}


extern "C" {
// libcouchbase handlers keep a C linkage...
static void error_callback(lcb_t instance,
                           lcb_error_t err, const char *errinfo)
{
    void *cookie = const_cast<void *>(lcb_get_cookie(instance));
    CouchbaseImpl *me = reinterpret_cast<CouchbaseImpl *>(cookie);
    me->errorCallback(err, errinfo);
}



static void get_callback(lcb_t,
                         const void *cookie,
                         lcb_error_t error,
                         const lcb_get_resp_t *resp)
{
    if (resp->version != 0) {
        unknownLibcouchbaseType("get", resp->version);
    }

    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);
}

static void store_callback(lcb_t,
                           const void *cookie,
                           lcb_storage_t,
                           lcb_error_t error,
                           const lcb_store_resp_t *resp)
{
    if (resp->version != 0) {
        unknownLibcouchbaseType("store", resp->version);
    }

    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);
}

static void arithmetic_callback(lcb_t,
                                const void *cookie,
                                lcb_error_t error,
                                const lcb_arithmetic_resp_t *resp)
{
    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);
}



static void remove_callback(lcb_t,
                            const void *cookie,
                            lcb_error_t error,
                            const lcb_remove_resp_t *resp)
{
    if (resp->version != 0) {
        unknownLibcouchbaseType("remove", resp->version);
    }

    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);

}

static void touch_callback(lcb_t,
                           const void *cookie,
                           lcb_error_t error,
                           const lcb_touch_resp_t *resp)
{
    if (resp->version != 0) {
        unknownLibcouchbaseType("touch", resp->version);
    }

    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);
}

static void http_complete_callback(lcb_http_request_t ,
                                   lcb_t ,
                                   const void *cookie,
                                   lcb_error_t error,
                                   const lcb_http_resp_t *resp)
{
    if (resp->version != 0) {
        unknownLibcouchbaseType("http_request", resp->version);
    }

    ResponseInfo ri(error, resp);
}

static void configuration_callback(lcb_t instance,
                                   lcb_configuration_t config)
{
    void *cookie = const_cast<void *>(lcb_get_cookie(instance));
    CouchbaseImpl *me = reinterpret_cast<CouchbaseImpl *>(cookie);
    me->onConnect(config);
}

static void unlock_callback(lcb_t,
                         const void *cookie,
                         lcb_error_t error,
                         const lcb_unlock_resp_t *resp)
{
    if (resp->version != 0) {
        unknownLibcouchbaseType("unlock", resp->version);
    }

    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);
}

static void durability_callback(lcb_t,
                                const void *cookie,
                                lcb_error_t error,
                                const lcb_durability_resp_t *resp)
{
    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);
}

} // extern "C"

void CouchbaseImpl::setupLibcouchbaseCallbacks(void)
{
    lcb_set_error_callback(instance, error_callback);
    lcb_set_get_callback(instance, get_callback);
    lcb_set_store_callback(instance, store_callback);
    lcb_set_arithmetic_callback(instance, arithmetic_callback);
    lcb_set_remove_callback(instance, remove_callback);
    lcb_set_touch_callback(instance, touch_callback);
    lcb_set_configuration_callback(instance, configuration_callback);
    lcb_set_http_complete_callback(instance, http_complete_callback);
    lcb_set_unlock_callback(instance, unlock_callback);
    lcb_set_durability_callback(instance, durability_callback);
}
