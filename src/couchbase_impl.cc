/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2012 Couchbase, Inc.
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
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "couchbase_impl.h"
#include "cas.h"
#include "logger.h"
#include <libcouchbase/libuv_io_opts.h>

using namespace std;
using namespace Couchnode;

Logger logger;

// libcouchbase handlers keep a C linkage...
extern "C" {
    // node.js will call the init method when the shared object
    // is successfully loaded. Time to register our class!
    static void init(Handle<Object> target)
    {
        CouchbaseImpl::Init(target);
    }

    NODE_MODULE(couchbase_impl, init)
}

#ifdef COUCHNODE_DEBUG
unsigned int CouchbaseImpl::objectCount;
#endif


static Handle<Value> bailOut(const Arguments &args, CBExc &ex)
{
    Handle<Function> cb;
    if (args.Length()) {
        cb = args[args.Length()-1].As<Function>();
    }

    if (cb.IsEmpty() || !cb->IsFunction()) {
        // no callback. must bail.
        return ex.throwV8();
    }

    Handle<Value> excObj = ex.asValue();
    TryCatch try_catch;
    cb->Call(v8::Context::GetEntered()->Global(), 1, &excObj);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    return v8::False();
}


CouchbaseImpl::CouchbaseImpl(lcb_t inst) :
    ObjectWrap(), connected(false), useHashtableParams(false),
    instance(inst), lastError(LCB_SUCCESS)

{
    lcb_set_cookie(instance, reinterpret_cast<void *>(this));
    setupLibcouchbaseCallbacks();
#ifdef COUCHNODE_DEBUG
    ++objectCount;
#endif
}

CouchbaseImpl::~CouchbaseImpl()
{
#ifdef COUCHNODE_DEBUG
    --objectCount;
    cerr << "Destroying handle.." << endl
         << "Still have " << objectCount << " handles remaining" << endl;
#endif
    if (instance) {
        lcb_destroy(instance);
    }

    EventMap::iterator iter = events.begin();
    while (iter != events.end()) {
        if (!iter->second.IsEmpty()) {
            iter->second.Dispose();
            iter->second.Clear();
        }
        ++iter;
    }
}

static Handle<Object> createConstants()
{
    Handle<Object> o = Object::New();
#define XCONSTANTS(X) \
    X(LCB_CNTL_SET) \
    X(LCB_CNTL_GET) \
    X(LCB_CNTL_OP_TIMEOUT) \
    X(LCB_CNTL_DURABILITY_INTERVAL) \
    X(LCB_CNTL_DURABILITY_TIMEOUT) \
    X(LCB_CNTL_HTTP_TIMEOUT) \
    X(LCB_CNTL_VIEW_TIMEOUT) \
    X(LCB_CNTL_RBUFSIZE) \
    X(LCB_CNTL_WBUFSIZE) \
    X(LCB_CNTL_CONFIGURATION_TIMEOUT) \
    X(CNTL_COUCHNODE_VERSION) \
    X(CNTL_LIBCOUCHBASE_VERSION) \
    X(CNTL_CLNODES) \
    X(CNTL_RESTURI)

#define X(n) \
    o->Set(String::NewSymbol(#n), Integer::New(n));

    XCONSTANTS(X)

#undef X
    return o;
}

void CouchbaseImpl::Init(Handle<Object> target)
{
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    Persistent<FunctionTemplate> s_ct;
    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("CouchbaseImpl"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "strError", StrError);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "on", On);
//    NODE_SET_PROTOTYPE_METHOD(s_ct, "view", View);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "shutdown", Shutdown);
//    NODE_SET_PROTOTYPE_METHOD(s_ct, "getDesignDoc", GetDesignDoc);
//    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDesignDoc", SetDesignDoc);
//    NODE_SET_PROTOTYPE_METHOD(s_ct, "deleteDesignDoc", DeleteDesignDoc);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setMultiEx", SetMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "addMultiEx", AddMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "replaceMultiEx", ReplaceMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "appendMultiEx", AppendMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getMultiEx", GetMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "lockMultiEx", LockMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "unlockMultiEx", UnlockMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "arithmeticMultiEx", ArithmeticMultiEx);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "_control", _Control);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "_connect", Connect);
    target->Set(String::NewSymbol("CouchbaseImpl"), s_ct->GetFunction());
    target->Set(String::NewSymbol("Constants"), createConstants());
    NameMap::initialize();
}

Handle<Value> CouchbaseImpl::On(const Arguments &args)
{
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsFunction()) {
        return ThrowException("Usage: cb.on('event', 'callback')");
    }

    // @todo verify that the user specifies a valid monitor ;)
    HandleScope scope;
    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
    Handle<Value> ret = me->on(args);
    return scope.Close(ret);
}

Handle<Value> CouchbaseImpl::on(const Arguments &args)
{
    HandleScope scope;
    Local<String> s = args[0]->ToString();
    char *func = new char[s->Length() + 1];
    memset(func, 0, s->Length() + 1);
    s->WriteAscii(func);

    string function(func);
    delete []func;

    EventMap::iterator iter = events.find(function);
    if (iter != events.end() && !iter->second.IsEmpty()) {
        iter->second.Dispose();
        iter->second.Clear();
    }

    events[function] =
        Persistent<Function>::New(
            Local<Function>::Cast(args[1]));

    return scope.Close(True());
}

Handle<Value> CouchbaseImpl::New(const Arguments &args)
{
    HandleScope scope;
    CBExc exc;

    if (args.Length() < 1) {
        return exc.eArguments("Need a URI").throwV8();
    }

    if (args.Length() > 4) {
        return exc.eArguments("Too many arguments").throwV8();
    }

    char *argv[4];
    lcb_error_t err;
    memset(argv, 0, sizeof(argv));

    for (int ii = 0; ii < args.Length(); ++ii) {
        if (args[ii]->IsString()) {
            Local<String> s = args[ii]->ToString();
            argv[ii] = new char[s->Length() + 1];
            s->WriteAscii(argv[ii]);

        } else if (!args[ii]->IsNull() && !args[ii]->IsUndefined()) {
            exc = CBExc("Incorrect arguments", args[ii]);
            return exc.throwV8();
        }
    }

    lcb_io_opt_st *iops;
    lcbuv_options_t iopsOptions;

    iopsOptions.version = 0;
    iopsOptions.v.v0.loop = uv_default_loop();
    iopsOptions.v.v0.startsop_noop = 1;

    err = lcb_create_libuv_io_opts(0, &iops, &iopsOptions);

    if (iops == NULL) {
        return exc.eLcb(err).throwV8();
    }


    lcb_create_st createOptions(argv[0], argv[1], argv[2], argv[3], iops);

    lcb_t instance;
    err = lcb_create(&instance, &createOptions);
    for (int ii = 0; ii < 4; ++ii) {
        delete[] argv[ii];
    }

    if (err != LCB_SUCCESS) {
        exc.eLcb(err).throwV8();
    }

    CouchbaseImpl *hw = new CouchbaseImpl(instance);
    hw->Wrap(args.This());
    return args.This();
}

Handle<Value> CouchbaseImpl::Connect(const Arguments &args)
{
    HandleScope scope;
    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
    lcb_error_t ec = lcb_connect(me->getLibcouchbaseHandle());
    if (ec != LCB_SUCCESS) {
        return CBExc().eLcb(ec).throwV8();
    }

    return scope.Close(True());
}

Handle<Value> CouchbaseImpl::StrError(const Arguments &args)
{
    HandleScope scope;

    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
    lcb_error_t errorCode = (lcb_error_t)args[0]->Int32Value();
    const char *errorStr = lcb_strerror(me->instance, errorCode);

    Local<String> result = String::New(errorStr);
    return scope.Close(result);
}

void CouchbaseImpl::errorCallback(lcb_error_t err, const char *errinfo)
{

    if (!connected) {
        // Time to fail out all the commands..
        connected = true;
    }

    if (err == LCB_ETIMEDOUT && onTimeout()) {
        return;
    }

    lastError = err;
    EventMap::iterator iter = events.find("error");
    if (iter != events.end() && !iter->second.IsEmpty()) {
        Local<Value> argv[1] = { Local<Value>::New(String::New(errinfo ? errinfo : "")) };
        iter->second->Call(Context::GetEntered()->Global(), 1, argv);
    }
}

void CouchbaseImpl::onConnect(lcb_configuration_t config)
{
    if (config == LCB_CONFIGURATION_NEW) {
        if (!connected) {
            connected = true;
        }
        runScheduledOperations();
    }

    lcb_set_configuration_callback(instance, NULL);

    EventMap::iterator iter = events.find("connect");
    if (iter != events.end() && !iter->second.IsEmpty()) {
        Local<Value> argv[1];
        iter->second->Call(Context::GetEntered()->Global(), 0, argv);
    }
}

bool CouchbaseImpl::onTimeout(void)
{
    EventMap::iterator iter = events.find("timeout");
    if (iter != events.end() && !iter->second.IsEmpty()) {
        Local<Value> argv[1];
        iter->second->Call(Context::GetEntered()->Global(), 0, argv);
        return true;
    }

    return false;
}

void CouchbaseImpl::runScheduledOperations()
{
    while (!pendingCommands.empty()) {
        Command *p = pendingCommands.front();
        lcb_error_t err = p->execute(getLibcouchbaseHandle());
        if (err != LCB_SUCCESS) {
            p->getCookie()->cancel(err);
        }
        pendingCommands.pop();
    }
}

// static
Handle<Value> CouchbaseImpl::makeOperation(const Arguments &args, Command &op)
{
    HandleScope scope;
    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());

    if (!op.initialize()) {
        return bailOut(args, op.getError());
    }

    if (!op.process()) {
        return bailOut(args, op.getError());
    }

    Cookie *cc = op.createCookie();
    cc->setParent(args.This());

    if (!me->connected) {
        // Schedule..
        Command *cp = op.copy();
        me->pendingCommands.push(cp);
        // Place into queue..
        return scope.Close(v8::True());

    } else {
        lcb_error_t err = op.execute(me->getLibcouchbaseHandle());

        if (err == LCB_SUCCESS) {
            return scope.Close(v8::True());

        } else {
            cc->cancel(err);
            return scope.Close(v8::False());
        }
    }
}

/*******************************************
 ** Entry point for all of the operations **
 ******************************************/

#define DEFINE_STOREOP(name, mode) \
Handle<Value> CouchbaseImpl::name##MultiEx(const Arguments &args) \
{ \
    StoreCommand op(args, mode, ARGMODE_MULTI); \
    return makeOperation(args, op); \
}

DEFINE_STOREOP(Set, LCB_SET)
DEFINE_STOREOP(Add, LCB_ADD)
DEFINE_STOREOP(Replace, LCB_REPLACE)
DEFINE_STOREOP(Append, LCB_APPEND)
DEFINE_STOREOP(Prepend, LCB_PREPEND)

Handle<Value> CouchbaseImpl::GetMultiEx(const Arguments &args)
{
    GetCommand op(args, ARGMODE_MULTI);
    return makeOperation(args, op);
}

Handle<Value> CouchbaseImpl::LockMultiEx(const Arguments &args)
{
    LockCommand op(args, ARGMODE_MULTI);
    return makeOperation(args, op);
}

Handle<Value> CouchbaseImpl::UnlockMultiEx(const Arguments &args)
{
    UnlockCommand op(args, ARGMODE_MULTI);
    return makeOperation(args, op);
}

Handle<Value> CouchbaseImpl::TouchMultiEx(const Arguments &args)
{
    TouchCommand op(args, ARGMODE_MULTI);
    return makeOperation(args, op);
}

Handle<Value> CouchbaseImpl::ArithmeticMultiEx(const Arguments &args)
{
    ArithmeticCommand op(args, ARGMODE_MULTI);
    return makeOperation(args, op);
}

Handle<Value> CouchbaseImpl::RemoveMultiEx(const Arguments &args)
{
    DeleteCommand op(args, ARGMODE_MULTI);
    return makeOperation(args, op);
}

//Handle<Value> CouchbaseImpl::View(const Arguments &args)
//{
//    HandleScope scope;
//    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
//    ViewOperation *op = new ViewOperation;
//    return makeOperation(me, args, op);
//}

Handle<Value> CouchbaseImpl::Shutdown(const Arguments &args)
{
    HandleScope scope;
    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
    me->shutdown();
    return scope.Close(True());
}

//Handle<Value> CouchbaseImpl::GetDesignDoc(const Arguments &args)
//{
//    HandleScope scope;
//    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
//    GetDesignDocOperation *op = new GetDesignDocOperation;
//    return makeOperation(me, args, op);
//}
//
//Handle<Value> CouchbaseImpl::SetDesignDoc(const Arguments &args)
//{
//    HandleScope scope;
//    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
//    SetDesignDocOperation *op = new SetDesignDocOperation;
//    return makeOperation(me, args, op);
//}
//
//Handle<Value> CouchbaseImpl::DeleteDesignDoc(const Arguments &args)
//{
//    HandleScope scope;
//    CouchbaseImpl *me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
//    DeleteDesignDocOperation *op = new DeleteDesignDocOperation;
//    return makeOperation(me, args, op);
//}
//
extern "C" {
    static void libuv_shutdown_cb(uv_timer_t* t, int) {
        ScopeLogger sl("libuv_shutdown_cb");
        lcb_t instance = (lcb_t)t->data;
        lcb_destroy(instance);
        delete t;
    }
}

void CouchbaseImpl::shutdown(void)
{
    if (!instance) {
        return;
    }

    uv_timer_t *timer = new uv_timer_t;
    uv_timer_init(uv_default_loop(), timer);
    timer->data = instance;
    instance = NULL;
    uv_timer_start(timer, libuv_shutdown_cb, 0, 0);
}


void CouchbaseImpl::dumpMemoryInfo(const std::string& mark="")
{
    HeapStatistics stats;
    return;

    V8::GetHeapStatistics(&stats);
    printf("%-20s: HEAP: Used %lu/%lu\n", mark.c_str(), stats.used_heap_size(),
           stats.total_heap_size());
}
