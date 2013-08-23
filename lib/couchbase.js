var couchnode = require("bindings")("couchbase_impl");
var util = require("util");
var CBpp = couchnode.CouchbaseImpl;
var CONST = couchnode.Constants;

/**
 * Constructor.
 * @param options a dictionary of options to use. Recognized options
 *  are:
 *      "host": a string or an array of strings indicating the hosts to
 *      connect to. If the value is an array, all the hosts in the array
 *      will be tried until one of them succeeds. Default is "localhost"
 *
 *      "bucket": the bucket to connect to. If not specified, the default is
 *      "default".
 *
 *      "username", "password": The credentials for the bucket.
 *
 *
 * Additionally, other options may be passed in this object which correspond
 * to the various settings (see their documentation). For example, it may
 * be helpful to set timeout properties *before* connection.
 *
 * @param callback a callback that will be invoked when the instance is actually
 * connected to the server.
 *
 * Note that it is safe to perform operations on the Couchbase object before
 * the connect callback is invoked. In this case, the operations are queued
 * until the connection is ready (or an unrecoverable error has taken place).
 */
function Couchbase(options, callback) {
    if (!callback) {
        callback = function(err) {
            if (!err) {
                return;
            }
            console.log("Error thrown from Couchbase constructor..");
            throw err;
        }
    };

    if (typeof options != "object") {
        callback("Options must be an object");
    }

    var ourObjs = {};
    for (var kName in options) {
        if (options.hasOwnProperty(kName)) {
            ourObjs[kName] = options[kName];
        }
    }

    options = ourObjs;


    cbArgs = new Array();
    if (options.host == undefined) {
        cbArgs[0] = "127.0.0.1:8091";
    } else if (typeof options.host == "array") {
        cbArgs[0] = options.host.join(";");
    } else {
        cbArgs[0] = options.host;
    }

    delete options['host'];

    // Bucket
    var argMap = {
        1: ["username", ""],
        2: ["password", ""],
        3: ["bucket", "default"]
    }

    for (var ix in argMap) {
        var spec = argMap[ix];
        var specName = spec[0];
        var specDefault = spec[1];
        var curValue = options[specName];

        if (!curValue) {
            curValue = specDefault;
        }
        cbArgs[ix] = curValue;

        delete options[specName];
    }

    try {
        this._cb = new CBpp(cbArgs[0], cbArgs[1], cbArgs[2], cbArgs[3]);
    } catch (e) {
        callback(e);
    }

    for (prefOption in options) {
        // Check that it exists:
        var prefValue = options[prefOption];
        if (typeof this[prefOption] == "undefined") {
            console.warn("Unknown option: " + prefOption);
        } else {
            this[prefOption](prefValue);
        }
    }

    this._cb.on("connect", callback);

    try {
        this._cb._connect();
    } catch(e) {
        callback(e);
    }
}


// Merge existing parameters into key-specific ones
function mergeParams(key, kParams, gParams) {
    var ret = {};
    ret[key] = kParams;
    for (opt in gParams) {
        // ignore the value parameter as it conflicts with our current one
        if (opt != "value") {
            kParams[opt] = gParams[opt];
        }
    }
    return ret;
}

Couchbase.prototype._invokeStorage = function(tgt, argList) {
    // (key, value, meta, callback)
    var meta = mergeParams(argList[0], { value: argList[1]}, argList[2]);
    tgt.call(this._cb, meta, null, argList[3]);
};

/******************************************************************************
 * General Function Protocol **************************************************
 * ****************************************************************************
 * Each of these function take as their last two parameters a "meta" argument
 * and a "callback" argument.
 *
 * The 'meta' argument affects various options for the operation itself; each
 * function will have different options for what goes into the meta. It is
 * passed as an Object to the function (and may be null)
 *
 * The callback itself is also invoked with (err, meta); where 'err' evaluates
 * to false if there is no error, and otherwise contains one of the following:
 *  1. An array of [string, object] if a fatal error occured during scheduling
 *     the operation
 *  2. An integer if the error failed due to network issues or a negative
 *     reply from the server.
 *
 * The second argument is an Object containing the details of the response.
 * Most operations will populate this with a "cas" property which contains
 * the opaque value representing the current state of the object. Each time
 * the object is mutated on the server, this value is modified.
 * If the 'cas' is supplied in the input meta, the server will check to see
 * that the cas provided is equal to the current CAS on the server. If they
 * do not match, the server assumes the user (i.e. you) does not want the
 * operation to succeed as the mutation supplied may be invalid (from an
 * application persspective), and would request for the operation to be
 * executed again.
 *
 * A common use case for utilizing this 'CAS' value is to simply pass
 * the meta received from the callback as the input meta to the next API,
 * so for example:
 *
 *  cb.get("a key", null, function(err, res) {
 *      if (err) {
 *          // handle errors here ..
            return;
        }

        var value = JSON.parse(res.value);
        // Mutate the meta. Assumes the value is an array:
        if (typeof value != "array") {
            console.error("Didn't get the right type...");
            return;
        }

        value.push("a new entry");
        cb.set("key", value, res)
    });
 *
 */

/**
 * Get a key from the cluster
 * @param key the key to retrieve
 * @param meta additional options for this operation
 * @param callback the callback to be invoked when complete
 *
 * Parameters accept for meta are:
 *  expiry: Also update the expiration time for the key.
 *
 */
Couchbase.prototype.get = function(key, meta, callback) {
    this._cb.getMulti([key], meta, callback);
};

/**
 * Update the item's expiration time in the cluster.
 * @param key the key to retrieve
 * @param meta additional options for this operation. This includes:
 *  'expiry' - the expiration time to use. If no value is provided, then
 *  the current expiration time is cleared and the key is set to never
 *  expire. Otherwise, the key is updated to expire in the value provided,
 *  in seconds
 */
Couchbase.prototype.touch = function(key, meta, callback) {
    this._cb.getMulti([key], meta, callback);
};

/**
 * Lock the object on the server and retrieve it. When an object is locked,
 * its CAS changes and subsequent operations on the object (without providing
 * the current CAS) will fail until the lock is no longer held
 *
 * @param key the item to lock
 * @param meta options. Options are:
 *  "expiry" - the duration of time the lock should be held for. If this value
 *  is not provided, it will use the default server-side lock duration which
 *  is 15 seconds. Note that the maximum duration for a lock is 30 seconds,
 *  and if a higher value is specified, it will be rounded to this number
 *
 * Once locked, an item can be unlocked either by explicitly calling unlock(),
 * or by performing a storage operation (e.g. set, replace, append) with
 * the current CAS value.
 */
Couchbase.prototype.lock = function(key, meta, callback) {
    this._cb.lockMulti([key], meta, callback);
};

/**
 * Delete a key on the server
 * @param key the key to remove
 * @param meta the options to use. Options are:
 *  "cas": the CAS value to check. If the item on the server contains a
 *  different CAS value, the operation will fail.
 */
Couchbase.prototype.delete = function(key, meta, callback) {
    this._cb.deleteMulti(mergeParams(key, {}, meta), null, callback);
};

/**
 * Unlock a previously locked item on the server
 * @param key the key to unlock
 * @param meta options. Note that this takes one option which MUST be
 *  provided, and this is the 'cas' to use for the unlock operation
 */
Couchbase.prototype.unlock = function(key, meta, callback) {
    this._cb.unlockMulti(mergeParams(key, {}, meta), null, callback);
};

/**
 * Store a key on the server, setting its value
 * @param key the key to store
 * @param value the value the key shall contain
 * @param meta extra options. Options are:
 *  - cas: (see above for description)
 *  - expiry: set initial expiration for the item
 *  - flags: 32 bit value to use for the item. Note that this value is
 *      not currently used by couchnode, but is used by other clients; and
 *      may be used by Couchnode in the future. The only use case for setting
 *      this value should be intra-client compatibility. Conventionally this
 *      value is used for indicating the storage format of the value.
 */
Couchbase.prototype.set = function(key, value, meta, callback) {
    this._invokeStorage(this._cb.setMulti, arguments);
};

/**
 * Like 'set', but will fail if the key already exists
 */
Couchbase.prototype.add = function(key, value, meta, callback) {
    this._invokeStorage(this._cb.addMulti, arguments);
};

/**
 * Like 'set', but will only succeed if the key exists (i.e.
 * the inverse of add())
 */
Couchbase.prototype.replace = function(key, meta, callback) {
    this._invokeStorage(this._cb.replaceMulti, arguments);
};

/**
 * Like 'set', but instead of setting a new value, it appends data
 * to the existing value. Note that this function only makes sense when
 * the stored item is a string; "appending" JSON may result in parse
 * errors when the value is later retrieved
 */
Couchbase.prototype.append = function(key, meta, callback) {
    this._invokeStorage(this._cb.appendMulti, arguments);
};

/**
 * Like 'append', but adds data to the beginning of the value
 */
Couchbase.prototype.prepend = function(key, meta, callback) {
    this._invokeStorage(this._cb.prependMulti, arguments);
};

/**
 * Increments the key's numeric value. This is an atomic operation
 * and more efficient than set() if the value is a number.
 *
 * Note that JavaScript does not support 64 bit integers (while libcouchbase
 * and the server does). You may end up receiving an invalid value if the
 * existing number is greater than 64 bits
 *
 * @param key the key to increment
 * @param meta options. Options are:
 *  'delta': The amount by which to increment; if not specified, the default is
 *      1
 *  'initial': The initial value to use if the key does not exist. If this is
 *      not supplied and the key does not exist,
 *
 *  'expiry': the expiration time for the key (may be null, in which case it is
 *      not used).
 *
 * Note that as this operation is atomic, no 'cas' parameter is provided.
 *
 * If the key already exists but its value is not numeric, an error will be
 * provided to the callback
 */
Couchbase.prototype.incr = function(key, meta, callback) {
    this._cb.arithmeticMulti(mergeParams(key, {delta:1}, meta), null, callback);
};

/**
 * Decrements the key's numeric value. Follows same semantics as 'incr',
 * with the exception that the 'delta' parameter is the amount by which to
 * decrement the existing value
 */
Couchbase.prototype.decr = function(key, meta, callback) {
    var ourParams = mergeParams(key, {}, meta);
    if (ourParams[key].meta) {
        ourParams[key].meta *= -1;
    } else {
        ourParams[key].meta = -1;
    }

    this._cb.arithmeticMulti(ourParams, null, callback);
};

/** Multi Methods */
Couchbase.prototype.setMulti = function(kv, meta, callback) {
    this._cb.setMulti.apply(this._cb, arguments);
};

Couchbase.prototype.addMulti = function(kv, meta, callback) {
    this._cb.addMulti.apply(this._cb, arguments);
};

Couchbase.prototype.replaceMulti = function(kv, meta, callback) {
    this._cb.replaceMulti.apply(this._cb, arguments);
};

Couchbase.prototype.appendMulti = function(kv, meta, callback) {
    this._cb.appendMulti.apply(this._cb, arguments);
};

Couchbase.prototype.prependMulti = function(kv, meta, callback) {
    this._cb.prependMulti.apply(this._cb, arguments);
};

Couchbase.prototype.getMulti = function(kv, meta, callback) {
    this._cb.getMulti.apply(this._cb, arguments);
};

Couchbase.prototype.lockMulti = function(kv, meta, callback) {
    this._cb.lockMulti.apply(this._cb, arguments);
};

Couchbase.prototype.unlockMulti = function(kv, meta, callback) {
    this._cb.unlockMulti.apply(this._cb, arguments);
};

/** Handy and Informational Functions */

Couchbase.prototype.shutdown = function() {
    this._cb.shutdown();
};

Couchbase.prototype._ctl = function(cc, argList) {
    if (argList.length == 1) {
        return this._cb._control(cc, CONST.LCB_CNTL_SET, argList[0]);
    } else if (argList.length == 0) {
        return this._cb._control(cc, CONST.LCB_CNTL_GET);
    } else {
        throw new Error("Function takes 0 or 1 arguments");
    }
};

/**
 * Sets or gets the operation timeout. The operation timeout is the time
 * that Couchbase will wait for a response from the server. If the response
 * is not received within this time frame, the operation is bailed out with
 * an error.
 *
 * If called with no arguments, returns the current value. If called with
 * a single argument, the value is updated
 * @param msecs the timeout in milliseconds
 */
Couchbase.prototype.operationTimeout = function(msecs) {
    return this._ctl(CONST.LCB_CNTL_OP_TIMEOUT, arguments);
};

/**
 * Sets or gets the connection timeout. This is the timeout value used when
 * connecting to the configuration port during the initial connection (in this
 * case, use this as a key in the "options" parameter in the constructor) and/or
 * when Couchbase attempts to reconnect in-situ (if the current connection
 * has failed)
 *
 * @param msecs the timeout in milliseconds
 */
Couchbase.prototype.connectionTimeout = function(msecs) {
    return this._ctl(CONST.LCB_CNTL_OP_TIMEOUT, arguments);
};

/**
 * Get information about the libcouchbase version being used.
 * @return an array of [versionNumber, versionString], where
 * @c versionNumber is a hexadecimal number of 0x021002 - in this
 * case indicating version 2.1.2.
 *
 * Depending on the build type, this might include some other information
 * not listed here.
 */
Couchbase.prototype.lcbVersion = function() {
    return this._ctl(CONST.CNTL_LIBCOUCHBASE_VERSION, arguments);
};


/**
 * Get information about the Couchnode version (i.e. this library)
 * @return an array of [versionNumber, versionString]
 */
Couchbase.prototype.clientVersion = function() {
    return this._ctl(CONST.CNTL_COUCHNODE_VERSION, arguments);
};

/**
 * Get an array of active nodes in the cluster
 */
Couchbase.prototype.serverNodes = function() {
    return this._ctl(CONST.CNTL_CLNODES, arguments);
};

module.exports = Couchbase;
