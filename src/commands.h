/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef COUCHNODE_COMMANDS_H
#define COUCHNODE_COMMANDS_H 1
#ifndef COUCHBASE_H
#error "Include couchbase.h before including this file"
#endif

#include "buflist.h"
#include "commandoptions.h"
#include <cstdio>
namespace Couchnode {
using namespace v8;


enum ArgMode {
    ARGMODE_SIMPLE = 0x0,
    ARGMODE_MULTI = 0x2,
};


#define CTOR_COMMON(cls) \
        cls(const Arguments &args, int mode) : Command(args, mode) {}

class Command
{
public:
    /**
     * This callback is passed to the handler for each operation. It receives
     * the following:
     * @param cmd the command context itself. This is prototyped as Command but
     *  will always be a subclass
     * @param k, n an allocated key and size specifier. These *must* be assigned
     *  first before anything, If an error occurrs, the CommandList implementation
     *  shall free it
     * @param dv a specific value for the command itself.. this is set to NULL
     *  unless the keys was an object itself.
     */
    typedef bool (*ItemHandler)(Command *cmd,
                                const char *k, size_t n,
                                Handle<Value> &dv,
                                unsigned int ix);

    typedef enum { ArrayKeys, ObjectKeys, SingleKey } KeysType;

    Command(const Arguments& args, int cmdMode) : apiArgs(args) {
        ncmds = 0;
        mode = cmdMode;
        cookie = NULL;
        kcollType = SingleKey;
    }

    virtual ~Command() {

    }

    virtual bool initialize();
    virtual lcb_error_t execute(lcb_t) = 0;
    virtual Command* copy() = 0;

    // Process and validate all commands, and convert them into LCB commands
    bool process(ItemHandler handler);
    bool process() { return process(getHandler()); }

    // Get the exception object, if present
    CBExc &getError() { return err; }

    Cookie *createCookie();
    Cookie *getCookie() { return cookie; }

    void detachCookie() { cookie = NULL; }

protected:
    bool getBufBackedString(const Handle<Value> &v, char **k, size_t *n);
    bool parseCommonOptions(const Handle<Object>&);
    virtual Parameters* getParams() = 0;
    virtual bool initCommandList() = 0;

    virtual ItemHandler getHandler() const = 0;
    const Arguments& apiArgs;

    Command(Command&);


    NAMED_OPTION(SpooledOption, BooleanOption, SPOOLED);
    // Callback parameters..
    SpooledOption isSpooled;
    CallableOption callback;

    Cookie *cookie;

    CBExc err;
    unsigned int ncmds;
    Local<Value> keys;
    BufferList bufs;

    // Set by subclasses:
    int mode; // MODE_* | MODE_* ...

    // Determined in flight
    KeysType kcollType;

private:
    bool processObject(Handle<Object>&);
    bool processArray(Handle<Array>&);
    bool processSingle(Handle<Value>&, Handle<Value>&, unsigned int);
};
class GetCommand : public Command
{

public:
    CTOR_COMMON(GetCommand)
    lcb_error_t execute(lcb_t);
    static bool handleSingle(Command *, const char *, size_t,
                             Handle<Value>&, unsigned int);

    virtual Command* copy() { return new GetCommand(*this); }

protected:
    Parameters* getParams() { return &globalOptions; }
    GetOptions globalOptions;
    CommandList<lcb_get_cmd_t> commands;
    ItemHandler getHandler() const { return handleSingle; }
    virtual bool initCommandList() { return commands.initialize(ncmds); }
};

class LockCommand : public GetCommand
{
public:
    LockCommand(const Arguments& origArgs, int mode)
        : GetCommand(origArgs, mode) {
    }

    bool initialize() {
        if (!GetCommand::initialize()) {
            return false;
        }
        globalOptions.lockTime.forceIsFound();
        return true;
    }
};


class StoreCommand : public Command
{
public:
    StoreCommand(const Arguments& origArgs, lcb_storage_t sop, int mode)
        : Command(origArgs, mode), op(sop) { }

    static bool handleSingle(Command*, const char *, size_t,
                             Handle<Value>&, unsigned int);

    lcb_error_t execute(lcb_t);
    virtual Command* copy() { return new StoreCommand(*this); }

protected:
    lcb_storage_t op;
    CommandList<lcb_store_cmd_t> commands;
    StoreOptions globalOptions;
    ItemHandler getHandler() const { return handleSingle; }
    Parameters* getParams() { return &globalOptions; }
    virtual bool initCommandList() { return commands.initialize(ncmds); }

};

class UnlockCommand : public Command
{
public:
    CTOR_COMMON(UnlockCommand)
    virtual Command *copy() { return new UnlockCommand(*this); }
    lcb_error_t execute(lcb_t);

protected:
    static bool handleSingle(Command *, const char *, size_t,
                             Handle<Value>&, unsigned int);
    CommandList<lcb_unlock_cmd_t> commands;
    UnlockOptions globalOptions;
    ItemHandler getHandler() const { return handleSingle; }
    Parameters * getParams() { return &globalOptions; }
    virtual bool initCommandList() { return commands.initialize(ncmds); }
};

class TouchCommand : public Command
{
public:
    TouchCommand(const Arguments& origArgs, int mode)
        : Command(origArgs, mode) { }
    static bool handleSingle(Command *, const char *, size_t,
                             Handle<Value>&, unsigned int);
    lcb_error_t execute(lcb_t);
    virtual Command *copy() { return new TouchCommand(*this); }

protected:
    CommandList<lcb_touch_cmd_t> commands;
    TouchOptions globalOptions;
    ItemHandler getHandler() const { return handleSingle; }
    Parameters* getParams() { return &globalOptions; }
    virtual bool initCommandList() { return commands.initialize(ncmds); }
};

class ArithmeticCommand : public Command
{

public:
    CTOR_COMMON(ArithmeticCommand)
    lcb_error_t execute(lcb_t);
    Command * copy() { return new ArithmeticCommand(*this); }
protected:
    static bool handleSingle(Command *, const char *, size_t,
                             Handle<Value>&, unsigned int);
    CommandList<lcb_arithmetic_cmd_t> commands;
    ArithmeticOptions globalOptions;
    ItemHandler getHandler() const { return handleSingle; }
    Parameters* getParams() { return &globalOptions; }
    virtual bool initCommandList() { return commands.initialize(ncmds); }
};


class DeleteCommand : public Command
{
public:
    CTOR_COMMON(DeleteCommand)
    lcb_error_t execute(lcb_t);
    Command *copy() { return new DeleteCommand(*this); }

protected:
    static bool handleSingle(Command *, const char *, size_t,
                             Handle<Value>&, unsigned int);
    CommandList<lcb_remove_cmd_t> commands;
    DeleteOptions globalOptions;
    ItemHandler getHandler() const { return handleSingle; }
    Parameters * getParams() { return &globalOptions; }
    virtual bool initCommandList() { return commands.initialize(ncmds); }
};

}

#endif
