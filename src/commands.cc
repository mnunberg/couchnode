#include "couchbase_impl.h"
namespace Couchnode {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Get                                                                      ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool GetCommand::handleSingle(Command *p,
                              const char *key, lcb_size_t nkey,
                              Handle<Value>, unsigned int ix)
{
    GetCommand *ctx = static_cast<GetCommand *>(p);
    GetOptions *keyOptions = &ctx->globalOptions;
    lcb_get_cmd_st *cmd = ctx->commands.getAt(ix);

    cmd->v.v0.key = key;
    cmd->v.v0.nkey = nkey;
    if (keyOptions->lockTime.isFound()) {
        cmd->v.v0.exptime = keyOptions->lockTime.v;
        cmd->v.v0.lock = 1;

    } else {
        cmd->v.v0.exptime = keyOptions->expTime.v;
    }

    return true;
}

lcb_error_t GetCommand::execute(lcb_t instance)
{
    return lcb_get(instance, cookie, commands.size(), commands.getList());
}

bool GetOptions::parseObject(const Handle<Object> options, CBExc &ex)
{
    ParamSlot *specs[] = { &expTime, &lockTime };
    return ParamSlot::parseAll(options, specs, 2, ex);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Set                                                                      ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool StoreCommand::handleSingle(Command *p, const char *key, size_t nkey,
                                Handle<Value> params, unsigned int ix)
{
    StoreCommand *ctx = static_cast<StoreCommand *>(p);
    lcb_store_cmd_t *cmd = ctx->commands.getAt(ix);
    StoreOptions kOptions;

    if (!params.IsEmpty()) {
        if (!kOptions.parseObject(params.As<Object>(), ctx->err)) {
            return false;
        }
        kOptions.isInitialized = true;

    } else {
        ctx->err.eArguments("Must have options for set");
        return false;
    }


    char *vbuf;
    size_t nvbuf;
    Handle<Value> s = kOptions.value.v;

    cmd->v.v0.key = key;
    cmd->v.v0.nkey = nkey;

    if (!ctx->getBufBackedString(s, &vbuf, &nvbuf)) {
        return false;
    }

    cmd->v.v0.bytes = vbuf;
    cmd->v.v0.nbytes = nvbuf;
    cmd->v.v0.cas = kOptions.cas.v;

    // exptime
    if (kOptions.exp.isFound()) {
        cmd->v.v0.exptime = kOptions.exp.v;
    } else {
        cmd->v.v0.exptime = ctx->globalOptions.exp.v;
    }

    // flags
    if (kOptions.flags.isFound()) {
        cmd->v.v0.flags = kOptions.flags.v;
    } else {
        cmd->v.v0.flags = ctx->globalOptions.flags.v;
    }

    cmd->v.v0.operation = ctx->op;
    return true;
}

lcb_error_t StoreCommand::execute(lcb_t instance)
{
    return lcb_store(instance, cookie, commands.size(), commands.getList());
}

bool StoreOptions::parseObject(const Handle<Object> options, CBExc &ex)
{
    ParamSlot *spec[] = { &cas, &exp, &flags, &value };
    if (!ParamSlot::parseAll(options, spec, 4, ex)) {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Arithmetic                                                               ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool ArithmeticOptions::parseObject(const Handle<Object> obj, CBExc &ex)
{
    ParamSlot *spec[] = { &this->exp, &initial, &delta };
    return ParamSlot::parseAll(obj, spec, 3, ex);
}

void ArithmeticOptions::merge(const ArithmeticOptions &other)
{
    if (!exp.isFound()) {
        exp = other.exp;
    }

    if (!initial.isFound()) {
        initial = other.initial;
    }

    if (!delta.isFound()) {
        delta = other.delta;
    }
}

bool ArithmeticCommand::handleSingle(Command *p, const char *k, size_t n,
                                     Handle<Value> params, unsigned int ix)
{
    ArithmeticOptions kOptions;
    ArithmeticCommand *ctx = static_cast<ArithmeticCommand *>(p);
    lcb_arithmetic_cmd_t *cmd = ctx->commands.getAt(ix);


    if (!params.IsEmpty()) {
        if (!kOptions.parseObject(params.As<Object>(), ctx->err)) {
            return false;
        }
        kOptions.isInitialized = true;
    }


    kOptions.merge(ctx->globalOptions);

    cmd->v.v0.key = k;
    cmd->v.v0.nkey = n;
    cmd->v.v0.delta = kOptions.delta.v;
    cmd->v.v0.initial = kOptions.initial.v;
    if (kOptions.initial.isFound()) {
        cmd->v.v0.create = 1;
    }
    cmd->v.v0.exptime = kOptions.exp.v;

    return true;
}

lcb_error_t ArithmeticCommand::execute(lcb_t instance)
{
    return lcb_arithmetic(instance, cookie, commands.size(), commands.getList());
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Delete                                                                   ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool DeleteCommand::handleSingle(Command *p, const char *k, size_t n,
                                 Handle<Value> params, unsigned int ix)
{
    DeleteOptions *effectiveOptions;
    DeleteCommand *ctx = static_cast<DeleteCommand*>(p);
    DeleteOptions kOptions;

    if (!params.IsEmpty()) {
        if (!kOptions.parseObject(params.As<Object>(), ctx->err)) {
            return false;
        }
        effectiveOptions = &kOptions;
    } else {
        effectiveOptions = &ctx->globalOptions;
    }

    lcb_remove_cmd_t *cmd = ctx->commands.getAt(ix);
    cmd->v.v0.key = k;
    cmd->v.v0.nkey = n;
    cmd->v.v0.cas = effectiveOptions->cas.v;
    return true;
}

lcb_error_t DeleteCommand::execute(lcb_t instance)
{
    return lcb_remove(instance, cookie, commands.size(), commands.getList());
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Unlock                                                                   ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool UnlockOptions::parseObject(const Handle<Object> obj, CBExc &ex)
{
    ParamSlot *spec = &cas;
    return ParamSlot::parseAll(obj, &spec, 1, ex);
}

bool UnlockCommand::handleSingle(Command *p, const char *k, size_t n,
                                 Handle<Value> params, unsigned int ix)
{
    UnlockCommand *ctx = static_cast<UnlockCommand*>(p);
    UnlockOptions kOptions;
    if (params.IsEmpty()) {
        ctx->err.eArguments("Unlock must have CAS");
        return false;
    }

    if (!kOptions.parseObject(params.As<Object>(), ctx->err)) {
        return false;
    }
    if (!kOptions.cas.isFound()) {
        ctx->err.eArguments("Unlock must have CAS");
        return false;
    }

    lcb_unlock_cmd_t *cmd = ctx->commands.getAt(ix);
    cmd->v.v0.key = k;
    cmd->v.v0.nkey = n;
    cmd->v.v0.cas = kOptions.cas.v;
    return true;
}

lcb_error_t UnlockCommand::execute(lcb_t instance)
{
    return lcb_unlock(instance, cookie, commands.size(), commands.getList());
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Touch                                                                    ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool TouchOptions::parseObject(const Handle<Object> obj, CBExc &ex)
{
    ParamSlot *spec = &this->exp;
    return ParamSlot::parseAll(obj, &spec, 1, ex);
}

bool TouchCommand::handleSingle(Command *p, const char *k, size_t n,
                                Handle<Value> params, unsigned int ix)
{
    TouchCommand *ctx = static_cast<TouchCommand *>(p);
    TouchOptions kOptions;
    if (!params.IsEmpty()) {
        if (!kOptions.parseObject(params.As<Object>(), ctx->err)) {
            return false;
        }
    } else {
        kOptions.exp = ctx->globalOptions.exp;
    }

    lcb_touch_cmd_t *cmd = ctx->commands.getAt(ix);
    cmd->v.v0.key = k;
    cmd->v.v0.nkey = n;
    cmd->v.v0.exptime = kOptions.exp.v;
    return true;
}

lcb_error_t TouchCommand::execute(lcb_t instance)
{
    return lcb_touch(instance, cookie, commands.size(), commands.getList());
}

}
