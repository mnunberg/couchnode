var couchnode = require("bindings")("couchbase_impl");
var util = require("util");
var CBpp = couchnode.CouchbaseImpl;
var CONST = couchnode.Constants;

/**
 * Constructor.
 */
function Couchbase(options, callback) {
    if (!callback) {
        callback = function(err) {
            throw new Error(err);
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
        delete options[spec];
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

function makeSetArgs(key, value, existingMeta) {
    var ourMeta = { value : value };


    for (var k in existingMeta) {
        ourMeta[k] = existingMeta[k];
    }

    var ret = {};
    ret[key] = ourMeta;
    return ret;
}

Couchbase.prototype.set = function(key, value, meta, callback) {
    this._cb.setMultiEx(makeSetArgs(key, value, meta), null, callback);
};

Couchbase.prototype.get = function(key, meta, callback) {
    this._cb.getMultiEx([key], meta, callback);
};

Couchbase.prototype.add = function(key, value, meta, callback) {
    this._cb.addMultiEx(makeSetArgs(key, value, meta), null, callback);
};

Couchbase.prototype.replace = function(key, meta, callback) {
    this._cb.replaceMultiEx(makeSetArgs(key, value, meta), null, callback);
};

Couchbase.prototype.append = function(key, meta, callback) {
    this._cb.appendMultiEx(makeSetArgs(key, value, meta), null, callback);
};

Couchbase.prototype.prepend = function(key, meta, callback) {
    this._cb.prependMultiEx(makeSetArgs(key, value, meta), null, callback);
};

Couchbase.prototype.getInternalHandler = function() {
    return this._cb;
};

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

Couchbase.prototype.operationTimeout = function(msecs) {
    return this._ctl(CONST.LCB_CNTL_OP_TIMEOUT, arguments);
};

Couchbase.prototype.connectionTimeout = function(msecs) {
    return this._ctl(CONST.LCB_CNTL_OP_TIMEOUT, arguments);
};

Couchbase.prototype.lcbVersion = function() {
    return this._ctl(CONST.CNTL_LIBCOUCHBASE_VERSION, arguments);
};

Couchbase.prototype.clientVersion = function() {
    return this._ctl(CONST.CNTL_COUCHNODE_VERSION, arguments);
};

Couchbase.prototype.serverNodes = function() {
    return this._ctl(CONST.CNTL_CLNODES);
};


/**
 * Example: Set up an instance and try to set a key..
 */

function runExample() {
    var cb = new Couchbase({
            host: "localhost",
            operationTimeout : 1500,
            connectionTimeout : 5000
        }, function(err) {
            if (err) { console.error("Got error on connection :" +err); }
    });
    
    cb.set("foo", "bar", null, function(err, result) {
        if (err) {
            console.error("Got an error :" + err);
        }
        
        console.log("Will now dump some exciting new information!");
        console.log("Libcouchbase Version: " + cb.lcbVersion());
        console.log("Couchnode Version: " + cb.clientVersion());
        console.log("Operation Timeout (msecs): " + cb.operationTimeout());
        console.log("Connection Timeout (msecs): " + cb.connectionTimeout());
        
        cb.shutdown();
    });
    
    cb.set("this will fail", null, null, function(err) { console.error("Got expected failure: " + err); });
}


runExample();