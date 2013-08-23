#include "couchbase_impl.h"

// Thanks mauke
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)

namespace Couchnode
{

Handle<Value> CouchbaseImpl::_Control(const Arguments &args)
{
    HandleScope scope;
    CBExc exc;
    CouchbaseImpl *me;
    lcb_t instance;
    lcb_error_t err;

    me = ObjectWrap::Unwrap<CouchbaseImpl>(args.This());
    instance = me->getLibcouchbaseHandle();

    if (args.Length() < 2) {
        return exc.eArguments("Too few arguments").throwV8();
    }

    int mode = args[0]->IntegerValue();
    int option = args[1]->IntegerValue();

    Handle<Value> optVal = args[2];

    if (option != LCB_CNTL_SET && option != LCB_CNTL_GET) {
        return exc.eArguments("Invalid option mode").throwV8();
    }

    if (option == LCB_CNTL_SET && optVal.IsEmpty()) {
        return exc.eArguments("Valid argument missing for 'CNTL_SET'").throwV8();
    }

    switch (mode) {

    case LCB_CNTL_CONFIGURATION_TIMEOUT:
    case LCB_CNTL_VIEW_TIMEOUT:
    case LCB_CNTL_HTTP_TIMEOUT:
    case LCB_CNTL_DURABILITY_INTERVAL:
    case LCB_CNTL_DURABILITY_TIMEOUT:
    case LCB_CNTL_OP_TIMEOUT:
    {
        lcb_uint32_t tmoval;
        if (option == LCB_CNTL_GET) {
            err =  lcb_cntl(instance, option, mode, &tmoval);
            if (err != LCB_SUCCESS) {
                return exc.eLcb(err).throwV8();
            } else {
                return Number::New(tmoval / 1000);
            }
        } else {
            tmoval = optVal->NumberValue() * 1000;
            err = lcb_cntl(instance, option, mode, &tmoval);
        }

        break;
    }

    case LCB_CNTL_WBUFSIZE:
    case LCB_CNTL_RBUFSIZE: {
        lcb_size_t bufszval;

        if (option == LCB_CNTL_GET) {
            err = lcb_cntl(instance, option, mode, &bufszval);
            if (err != LCB_SUCCESS) {
                return exc.eLcb(err).throwV8();
            } else {
                return scope.Close(Number::New(bufszval));
            }
        } else {
            bufszval = optVal->Uint32Value();
            err = lcb_cntl(instance, option, mode, &bufszval);
        }
        break;
    }

    case CNTL_COUCHNODE_VERSION: {
        // always return something useful
        Handle<Array> ret = Array::New(2);
        ret->Set(0, Integer::New(0x002000));
        ret->Set(1, String::New("Couchnode v0.20.0"));
        return scope.Close(ret);
    }

    case CNTL_LIBCOUCHBASE_VERSION: {
        const char *vstr;
        lcb_uint32_t vnum;
        vstr = lcb_get_version(&vnum);

        Handle<Array> ret = Array::New(4);
        ret->Set(0, Integer::New(vnum));
        ret->Set(1, String::New(vstr));

        // Version and changeset of the headers
        ret->Set(2, String::New("HDR(VERSION): "LCB_VERSION_STRING));
        ret->Set(3, String::New("HDR(CHANGESET): "STRINGIFY(LCB_VERSION_CHANGESET)));
        return scope.Close(ret);
    }

    case CNTL_CLNODES: {
        const char * const * l;
        const char * const *cur;
        l = lcb_get_server_list(instance);
        unsigned int nItems = 0;
        for (cur = l; *cur; cur++) {
            nItems++;
        }

        Handle<Array> arr = Array::New(nItems);
        for (nItems = 0, cur = l; *cur; cur++, nItems++) {
            Handle<Value> s = String::New(*cur);
            arr->Set(nItems, s);
        }

        return scope.Close(arr);
    }

    case CNTL_RESTURI: {
        const char *s = lcb_get_host(instance);
        return scope.Close(String::New(s));
    }


    default:
        return exc.eArguments("Not supported yet").throwV8();
    }

    if (err == LCB_SUCCESS) {
        return scope.Close(v8::True());
    } else {
        return exc.eLcb(err).throwV8();
    }
}

}
