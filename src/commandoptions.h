/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef COUCHNODE_COMMANDOPTIONS_H
#define COUCHNODE_COMMANDOPTIONS_H 1
#ifndef COUCHBASE_H
#error "Include couchbase.h before including this file"
#endif

namespace Couchnode {

#define NAMED_OPTION(name, base, fld) \
    struct name : base \
    { \
        virtual Handle<String> getName() const { \
            return NameMap::names[NameMap::fld]; \
        } \
    }

struct Parameters
{
    // Pure virtual functions
    virtual bool parseObject(const Handle<Object> obj, CBExc &) = 0;
    bool isInitialized;
};

struct GetOptions : Parameters
{
    ExpOption expTime;
    ExpOption lockTime;
    bool parseObject(const Handle<Object> opts, CBExc &ex);
};

struct StoreOptions : Parameters
{
    CasSlot cas;
    ExpOption exp;
    FlagsOption flags;
    ValueOption value;
    bool parseObject(const Handle<Object> opts, CBExc &ex);
};

struct UnlockOptions : Parameters
{
    CasSlot cas;
    bool parseObject(const Handle<Object> , CBExc &ex);
};

struct DeleteOptions : UnlockOptions
{
};

struct TouchOptions : Parameters
{
    ExpOption exp;
    bool parseObject(const Handle<Object>, CBExc &);
};


struct DurabilityOptions : Parameters
{
    NAMED_OPTION(PersistToOption, Int32Option, PERSIST_TO);
    NAMED_OPTION(ReplicateToOption, Int32Option, REPLICATE_TO);
    NAMED_OPTION(TimeoutOption, UInt32Option, TIMEOUT);

    // Members
    PersistToOption persist_to;
    ReplicateToOption replicate_to;
    CasSlot cas;
    TimeoutOption timeout;
};

struct ArithmeticOptions : Parameters
{
    NAMED_OPTION(InitialOption, UInt64Option, INITIAL);
    NAMED_OPTION(DeltaOption, Int64Option, DELTA);

    ExpOption exp;
    InitialOption initial;
    DeltaOption delta;
    bool parseObject(const Handle<Object>, CBExc&);

    void merge(const ArithmeticOptions& other);
};

}

#endif
