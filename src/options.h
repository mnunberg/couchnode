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
#ifndef COUCHNODE_OPTIONS_H
#define COUCHNODE_OPTIONS_H 1
#ifndef COUCHBASE_H
#error "include couchbase_impl.h first"
#endif

#include <cstdlib>

namespace Couchnode {
// This file contains routines to parse parameters from the user.

enum ParseStatus {
    PARSE_OPTION_EMPTY,
    PARSE_OPTION_ERROR,
    PARSE_OPTION_FOUND
};

struct ParamSlot {
    static ParseStatus validateNumber(const Handle<Value>, CBExc&);
    virtual ParseStatus parseValue(const Handle<Value>, CBExc&) = 0;
    virtual Handle<String> getName() const = 0;
    ParamSlot() : status(PARSE_OPTION_EMPTY) { }
    virtual ~ParamSlot() { }

    ParseStatus returnStatus(ParseStatus s) {
        status = s;
        return s;
    }

    bool isFound() const { return status == PARSE_OPTION_FOUND; }
    void forceIsFound() { status = PARSE_OPTION_FOUND; }

    ParseStatus status;

    static bool parseAll(const Handle<Object>, ParamSlot **, size_t, CBExc&);
};

struct CasSlot : ParamSlot
{
    lcb_cas_t v;
    CasSlot() : v(0) {}
    virtual Handle<String> getName() const {
        return NameMap::names[NameMap::CAS];
    }

    virtual ParseStatus parseValue(const Handle<Value>, CBExc &);
};

struct ExpOption : ParamSlot
{
    uint32_t v;
    virtual ParseStatus parseValue(const Handle<Value>, CBExc &);
    ExpOption() : v(0) {}

    virtual Handle<String> getName() const {
        return NameMap::names[NameMap::EXPIRY];
    }
};

struct LockOption : ExpOption {
    virtual Handle<String> getName() const {
        return NameMap::names[NameMap::LOCKTIME];
    }
};

struct FlagsOption : ParamSlot
{
    uint32_t v;
    FlagsOption() : v(0) { }
    virtual ParseStatus parseValue(const Handle<Value> val, CBExc &ex);
    virtual Handle<String> getName() const {
        return NameMap::names[NameMap::FLAGS];
    }
};

struct Int64Option : ParamSlot
{
    int64_t v;
    Int64Option() : v(0) {}
    virtual ParseStatus parseValue(const Handle<Value> , CBExc&);
};

struct Uint64Option : ParamSlot
{
    uint64_t v;
    Uint64Option() :v(0) {}
    virtual ParseStatus parseValue(const Handle<Value> , CBExc&);
};

struct BooleanOption : ParamSlot
{
    bool v;
    BooleanOption() :v(false) {}
    virtual ParseStatus parseValue(const Handle<Value> val, CBExc&) {
        v = val->BooleanValue();
        return returnStatus(PARSE_OPTION_FOUND);
    }
};

struct CallableOption : ParamSlot
{
    Handle<Function> v;
    virtual ParseStatus parseValue(const Handle<Value> val, CBExc& ex);

    virtual Handle<String> getName() const {
        abort();
        static Handle<String> h;
        return h;
    }
};

struct StringOption : ParamSlot
{
    Local<Value> v;
    virtual ParseStatus parseValue(const Handle<Value> val, CBExc& ex);
};

struct KeyOption : StringOption
{
    virtual Handle<String> getName() const {
        return NameMap::names[NameMap::KEY];
    }
};

struct ValueOption : StringOption
{
    virtual Handle<String> getName() const {
        return NameMap::names[NameMap::VALUE];
    }

    // todo: convert to JSON
};

};

#endif
