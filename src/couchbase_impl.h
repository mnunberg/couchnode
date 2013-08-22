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
#ifndef COUCHBASE_H
#define COUCHBASE_H 1

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#pragma warning(disable : 4506)
#pragma warning(disable : 4530)
#endif

#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

// Unfortunately the v8 header spits out a lot of warnings..
// let's disable them..
#if __GNUC__
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#endif

#include <node.h>

#if __GNUC__
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic pop
#endif
#endif

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <queue>
#include <libcouchbase/couchbase.h>
#include <node.h>

#include "cas.h"
#include "exception.h"
#include "namemap.h"
#include "cookie.h"
#include "options.h"
#include "commandlist.h"
#include "commands.h"
#include "namespace_includes.h"
namespace Couchnode
{


class QueuedCommand;

class CouchbaseImpl: public node::ObjectWrap
{
public:
    CouchbaseImpl(lcb_t inst);
    virtual ~CouchbaseImpl();

    // Methods called directly from JavaScript
    static void Init(Handle<Object> target);
    static Handle<Value> New(const Arguments &args);
    static Handle<Value> StrError(const Arguments &args);
    static Handle<Value> GetVersion(const Arguments &);
    static Handle<Value> SetTimeout(const Arguments &);
    static Handle<Value> GetTimeout(const Arguments &);
    static Handle<Value> GetRestUri(const Arguments &);
    static Handle<Value> SetSynchronous(const Arguments &);
    static Handle<Value> IsSynchronous(const Arguments &);
    static Handle<Value> SetHandler(const Arguments &);
    static Handle<Value> GetLastError(const Arguments &);
    static Handle<Value> GetMultiEx(const Arguments &);
    static Handle<Value> LockMultiEx(const Arguments &);
    static Handle<Value> SetMultiEx(const Arguments &);
    static Handle<Value> ReplaceMultiEx(const Arguments &);
    static Handle<Value> AddMultiEx(const Arguments &);
    static Handle<Value> AppendMultiEx(const Arguments &);
    static Handle<Value> PrependMultiEx(const Arguments &);
    static Handle<Value> RemoveMultiEx(const Arguments &);
    static Handle<Value> ArithmeticMultiEx(const Arguments &);
    static Handle<Value> TouchMultiEx(const Arguments &);
    static Handle<Value> UnlockMultiEx(const Arguments &);
    static Handle<Value> View(const Arguments &);
    static Handle<Value> Shutdown(const Arguments &);

    // Design Doc Management
    static Handle<Value> GetDesignDoc(const Arguments &);
    static Handle<Value> SetDesignDoc(const Arguments &);
    static Handle<Value> DeleteDesignDoc(const Arguments &);


    // Setting up the event emitter
    static Handle<Value> On(const Arguments &);
    Handle<Value> on(const Arguments &);

    // Method called from libcouchbase
    void onConnect(lcb_configuration_t config);
    bool onTimeout(void);

    void errorCallback(lcb_error_t err, const char *errinfo);
    void runScheduledOperations();

    void shutdown(void);

    lcb_t getLibcouchbaseHandle(void) {
        return instance;
    }

    bool isConnected(void) const {
        return connected;
    }

    bool isUsingHashtableParams(void) const {
        return useHashtableParams;
    }
protected:
    bool connected;
    bool useHashtableParams;
    lcb_t instance;
    lcb_error_t lastError;

    typedef std::map<std::string, Persistent<Function> > EventMap;
    EventMap events;
    Persistent<Function> connectHandler;
    std::queue<Command *> pendingCommands;
    void setupLibcouchbaseCallbacks(void);

#ifdef COUCHNODE_DEBUG
    static unsigned int objectCount;
#endif
private:
    static Handle<Value> makeOperation(const Arguments&, Command&);
};


Handle<Value> ThrowException(const char *str);
Handle<Value> ThrowIllegalArgumentsException(void);
} // namespace Couchnode

#endif
