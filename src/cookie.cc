/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2013 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

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
    Handle<Object> payload = info.payload;
    if (payload.IsEmpty()) {
        payload = Object::New();
    }

    if (!ec->IsUndefined()) {
        info.setField(NameMap::ERROR, ec);
    }

    spooledInfo->ForceSet(info.getKey(), payload);
}

void Cookie::invokeSingleCallback(Handle<Value>& errObj, ResponseInfo& info)
{
    Handle<Value> args[2] = { errObj, info.payload };
    int argc = 2;
    if (args[1].IsEmpty()) {
        argc = 1;
    }

    TryCatch try_catch;
    callback->Call(v8::Context::GetEntered()->Global(), argc, args);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

void Cookie::invokeSpooledCallback()
{
    Handle<Value> globalErr;
    if (hasError) {
        CBExc ex;
        ex.assign(ErrorCode::CHECK_RESULTS);
        globalErr = ex.asValue();
    } else {
        globalErr = v8::Undefined();
    }

    Handle<Value> args[2] = { globalErr, spooledInfo };
    TryCatch try_catch;
    callback->Call(v8::Context::GetEntered()->Global(), 2, args);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
}

bool Cookie::hasRemaining() {
    return remaining;
}


void Cookie::markProgress(ResponseInfo &info) {
    remaining--;
    Handle<Value> errObj;

    if (isCancelled == false && info.hasKey() == false) {
        // Termination via 'NULL'
        if (cbType == CBMODE_SPOOLED) {
            invokeSpooledCallback();
            delete this;
        }
    }

    if (info.status != LCB_SUCCESS) {
        hasError = true;
        errObj = CBExc().eLcb(info.status).asValue();
    } else {
        errObj = v8::Undefined();
    }

    if (cbType == CBMODE_SINGLE) {
        invokeSingleCallback(errObj, info);
    } else {
        addSpooledInfo(errObj, info);
    }

    if (remaining == 0 && cbType == CBMODE_SPOOLED) {
        invokeSpooledCallback();
    }

    if (!hasRemaining()) {
        delete this;
    }
}

void Cookie::cancel(lcb_error_t err, Handle<Array> keys)
{
    isCancelled = 1;
    for (unsigned int ii = 0; ii < keys->Length(); ii++) {
        Handle<Value> key = keys->Get(ii);
        ResponseInfo ri(err, key);
        markProgress(ri);
    }
}

void StatsCookie::invoke(lcb_error_t err)
{
    HandleScope scope;
    Handle<Value> errObj;

    if (err != LCB_SUCCESS) {
        errObj = CBExc().eLcb(err).asValue();
    } else {
        errObj = v8::Undefined();
    }

    Handle<Value> argv[2] = { errObj, spooledInfo };
    if (spooledInfo.IsEmpty()) {
        argv[1] = Object::New();
    }

    v8::TryCatch try_catch;
    callback->Call(v8::Context::GetEntered()->Global(), 2, argv);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
    printf("Deleting %p\n", this);
    delete this;
}

void StatsCookie::update(lcb_error_t err,
                         const lcb_server_stat_resp_t *resp)
{
    HandleScope scope;
    if (err != LCB_SUCCESS && lastError == LCB_SUCCESS) {
        lastError = err;
    }

    if (resp == NULL || resp->v.v0.server_endpoint == NULL) {
        invoke(lastError);
        return;
    }

    // Otherwise, add to the information
    Handle<String> serverString = String::New(resp->v.v0.server_endpoint);
    Handle<Object> serverStats;
    if (!spooledInfo->Has(serverString)) {
        serverStats = Object::New();
        spooledInfo->ForceSet(serverString, serverStats);
    } else {
        serverStats = spooledInfo->Get(serverString).As<Object>();
    }

    Handle<String> statsKey = String::New((const char *)resp->v.v0.key,
                                          resp->v.v0.nkey);
    Handle<String> statsValue = String::New((const char *)resp->v.v0.bytes,
                                            resp->v.v0.nbytes);
    serverStats->ForceSet(statsKey, statsValue);
}

void StatsCookie::cancel(lcb_error_t err, Handle<Array>)
{
    invoke(err);
}

void HttpCookie::update(lcb_error_t err, const lcb_http_resp_t *resp)
{
    HandleScope scope;
    Handle<Value> errObj;
    v8::TryCatch try_catch;

    if (err) {
        errObj = CBExc().eLcb(err).asValue();
    } else {
        errObj = v8::Undefined();
    }


    if (!resp) {
        // Cancellation
        Handle<Value> args[] = { errObj };
        callback->Call(v8::Context::GetEntered()->Global(), 1, args);
        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
            delete this;
            return;
        }
    }

    Handle<Object> payload = Object::New();
    payload->ForceSet(NameMap::names[NameMap::HTTP_STATUS],
                      Number::New(resp->v.v0.status));

    if (err != LCB_SUCCESS) {
        payload->ForceSet(NameMap::names[NameMap::ERROR], errObj);
    }

    if (resp->v.v0.nbytes) {
        Handle<Value> body = String::New((const char *)resp->v.v0.bytes,
                                          resp->v.v0.nbytes);
        if (body.IsEmpty()) {
            // binary?
            body = node::Encode(resp->v.v0.bytes, resp->v.v0.nbytes);
        }
        payload->ForceSet(NameMap::names[NameMap::HTTP_CONTENT], body);
    }

    if (resp->v.v0.path) {
        payload->ForceSet(NameMap::names[NameMap::HTTP_PATH],
                          String::New((const char *)resp->v.v0.path,
                                      resp->v.v0.npath));
    }

    Handle<Value> args[] = { errObj, payload };
    callback->Call(v8::Context::GetEntered()->Global(), 2, args);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    delete this;
}


template <typename T>
void initCommonInfo_v0(ResponseInfo *tp, lcb_error_t err, const T* resp)
{
    tp->key = resp->v.v0.key;
    tp->nkey = resp->v.v0.nkey;
    tp->status = err;
    tp->payload = Object::New();
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_get_resp_t *resp)
{
    initCommonInfo_v0(this, err, resp);
    if (err != LCB_SUCCESS) {
        return;
    }

    setCas(resp->v.v0.cas);
    setField(NameMap::FLAGS, Uint32::New(resp->v.v0.flags));

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

    setField(NameMap::HTCODE, Number::New(resp->v.v0.status));

}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_observe_resp_t *resp)
{
    status = err;
    if (resp->v.v0.key == NULL && resp->v.v0.nkey == 0) {
        return;
    }

    initCommonInfo_v0(this, err, resp);
    setField(NameMap::OBS_CODE, Number::New(resp->v.v0.status));

    setCas(resp->v.v0.cas);

    if (resp->v.v0.from_master) {
        setField(NameMap::OBS_ISMASTER, v8::True());
    }
}

ResponseInfo::ResponseInfo(lcb_error_t err, const lcb_durability_resp_t *resp)
{
    status = err;
    initCommonInfo_v0(this, err, resp);
    if (err == LCB_SUCCESS) {
        status = resp->v.v0.err;
    }

    if (resp->v.v0.exists_master) {
        setField(NameMap::DUR_FOUND_MASTER, v8::True());
    }

    if (resp->v.v0.persisted_master) {
        setField(NameMap::DUR_PERSISTED_MASTER, v8::True());
    }

    setField(NameMap::DUR_NPERSISTED, Number::New(resp->v.v0.npersisted));
    setField(NameMap::DUR_NREPLICATED, Number::New(resp->v.v0.nreplicated));

    setCas(resp->v.v0.cas);
}

ResponseInfo::ResponseInfo(lcb_error_t err, Handle<Value> kObj) :
        key(NULL), nkey(0), keyObj(kObj)
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

static void observe_callback(lcb_t,
                             const void *cookie,
                             lcb_error_t error,
                             const lcb_observe_resp_t *resp)
{
    ResponseInfo ri(error, resp);
    getInstance(cookie)->markProgress(ri);
}

static void stats_callback(lcb_t,
                           const void *cookie,
                           lcb_error_t error,
                           const lcb_server_stat_resp_t *resp)
{
    StatsCookie *sc =
            reinterpret_cast<StatsCookie *>(
                    const_cast<void *>(cookie));
    sc->update(error, resp);
}

static void http_complete_callback(lcb_http_request_t,
                                   lcb_t,
                                   const void *cookie,
                                   lcb_error_t error,
                                   const lcb_http_resp_t *resp)
{
    HttpCookie *hc =
            reinterpret_cast<HttpCookie *>(
                    const_cast<void *>(cookie));
    hc->update(error, resp);
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
    lcb_set_observe_callback(instance, observe_callback);
    lcb_set_stat_callback(instance, stats_callback);
}
