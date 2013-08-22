var qs = require("querystring");

exports.create = function(connection, config, ready) {
    // We aren't using prototype inheritance here for two reasons:
    //
    // 1) even though they may be faster, this constructor won't be
    //    called frequently
    //
    // 2) doing it this way means we get better inspectability in
    //    the repl, etc.

    var doJsonConversions = config.doJsonConversions != false;
    var getHandler = doJsonConversions ? getParsedHandler : getRawHandler;

    /**
     * Register a new event emitter
     *
     * @param event The name of the event to register the listener
     * @param callback The callback for the event. The callback function
     *                 takes a different set of arguments depending on
     *                 the callback.
     * @todo document the legal set of callback
     * @todo throw exeption for unknown callbacks
     */
    function on(event, callback) {
        if (config.debug) console.log("couchbase.bucket.on()");
        connection.on(event, callback);
    }

    function setEx(key, value, meta, callback) {
        var ourMeta = {
            cas: meta ? meta.cas : 0,
            value: value
        };

        var kv = {};
        kv[key] = ourMeta;

        connection.setMultiEx(
            kv,
            meta,
            callback
        );


        meta = null;
        ourMeta = null;
        key = null;
        value = null;
        callback = null;
    }

    function getEx(key, meta, callback) {
        return connection.getMultiEx([key], meta, callback);
    }

    function getMultiEx(keys, meta, callback) {
        return connection.getMultiEx(keys, meta, callback);
    }

    function setMultiEx(kv, meta, callback) {
        return connection.setMultiEx(kv, meta, callback);
    }

    function shutdown() {
        if (config.debug) console.log("couchbase.bucket.shutdown()");
        connection.shutdown();
    }

    function getVersion() {
        if (config.debug) console.log("couchbase.bucket.getVersion()");
        return connection.getVersion();
    }

    function strError(errorCode) {
        if (config.debug) console.log("couchbase.bucket.strError()");
        return connection.strError(errorCode);
    }

    function view(ddoc, name, query, callback) {
        if (config.debug) console.log("couchbase.bucket.view("+ddoc+","+name+")");

        var fields = ["descending", "endkey", "endkey_docid",
            "full_set", "group", "group_level", "inclusive_end",
            "key", "keys", "limit", "on_error", "reduce", "skip",
            "stale", "startkey", "startkey_docid"];
        var jsonFields = ["endkey", "key", "keys", "startkey"];

        for (var q in query) {
            if (fields.indexOf(q) == -1) {
                delete query[q];
            } else if (jsonFields.indexOf(q) != -1) {
                query[q] = JSON.stringify(query[q]);
            }
        }

        if (typeof ddoc != "string") {
            throw new Error("Design document must be specified as a string");
        }

        if (typeof name != "string") {
            throw new Error("View must be specified as a string");
        }


        connection.view(ddoc, name + "?" + qs.stringify(query),
                        couchnodeRestHandler, [callback, this]);
    }

    function setDesignDoc(name, data, callback) {
        if (config.debug) console.log("couchbase.bucket.setDesignDoc("+name+")");
        if (typeof name != "string") {
            throw new Error("Design document must be specified as a string");
        }

        connection.setDesignDoc(name, makeDoc(data),
                                couchnodeRestHandler, [callback, this]);
    }

    function getDesignDoc(name, callback) {
        if (config.debug) console.log("couchbase.bucket.getDesignDoc("+name+")");
        if (typeof name != "string") {
            throw new Error("Design document must be specified as a string");
        }
        connection.getDesignDoc(name, couchnodeRestHandler, [callback, this]);
    }

    function deleteDesignDoc(name, callback) {
        if (config.debug) console.log("couchbase.bucket.deleteDesignDoc("+name+")");
        if (typeof name != "string") {
            throw new Error("Design document must be specified as a string");
        }
        connection.deleteDesignDoc(name, couchnodeRestHandler, [callback, this]);
    }

    ready({
        on : on,
        shutdown : shutdown,
        setDesignDoc: setDesignDoc,
        getDesignDoc: getDesignDoc,
        deleteDesignDoc: deleteDesignDoc,
        getVersion: getVersion,
        strError: strError,
        setEx : setEx,
        getEx : getEx,
        getMultiEx : getMultiEx,
        setMultiEx : setMultiEx
    });
};
function requiredArgs() {
    for (var i = 0; i < arguments.length; i++) {
        if (typeof arguments[i] == "undefined") {
            throw new ReferenceError("missing required argument");
        }
    }
}

function makeDoc(doc) {
    if (typeof doc == "string") {
        return doc;
    } else {
        return JSON.stringify(doc);
    }
}

function makeError(conn, errorCode) {
    // Early-out for success
    if (errorCode == 0) {
        return null;
    }

    // Build a standard NodeJS Error object with the passed errorCode
    var errObj = new Error(conn.strError(errorCode));
    errObj.code = errorCode;
    return errObj;
}

// convert the c-based set callback format to something sane
function setHandler(data, errorCode, key, cas) {
    var error = makeError(data[1], errorCode);
    data[0](error, {
        id : key,
        cas : cas
    });
}

function arithmeticHandler(data, errorCode, key, cas, value) {
    var error = makeError(data[1], errorCode);
    data[0](error, value, {
        id: key,
        cas: cas
    });
}

function observeHandler(data, errorCode, key, cas, status, from_master, ttp, ttr) {
    var error = makeError(data[1], errorCode);
    if (key) {
        data[0](error, {
            id: key,
            cas: cas,
            status: status,
            from_master: from_master,
            ttp: ttp,
            ttr: ttr
        });
    } else {
        data[0](error, null);
    }
}

function getParsedHandler(data, errorCode, key, cas, flags, value) {
    // if it looks like it might be JSON, try to parse it
    if (/[\{\[]/.test(value)) {
        try {
            value = JSON.parse(value);
        } catch (e) {
        // console.log("JSON.parse error", e, value)
        }
    }
    var error = makeError(data[1], errorCode);
    data[0](error, value, {
        id : key,
        cas: cas,
        flags : flags
    });
}

function getRawHandler(data, errorCode, key, cas, flags, value) {

    var error = makeError(data[1], errorCode);
    data[0](error, value, {
        id : key,
        cas: cas,
        flags : flags
    });
}

function unlockHandler(data, errorCode, key) {
    var error = makeError(data[1], errorCode);
    data[0](error, {
        id: key
    });
}

function touchHandler(data, errorCode, key) {
    var error = makeError(data[1], errorCode);
    data[0](error, {
        id: key
    });
}

function removeHandler(data, errorCode, key, cas) {
    var error = makeError(data[1], errorCode);
    data[0](error, {
        id : key,
        cas: cas
    });
}

function appendHandler(data, errorCode, key, cas) {
    var error = makeError(data[1], errorCode);
    data[0](error, {id: key, cas: cas});
}

function prependHandler(data, errorCode, key, cas) {
    var error = makeError(data[1], errorCode);
    data[0](error, {id: key, cas: cas});
}

function couchnodeRestHandler(data, errorCode, httpError, body) {
    var error = makeError(data[1], errorCode);
    if (errorCode == 0 && (httpError < 200 || httpError > 299)) {
        // Build a standard NodeJS Error object with the passed errorCode
        error = new Error("HTTP error " + httpError);
        error.code = 0x0a;
    }

    try {
        body = JSON.parse(body)
    } catch (err) { }

    if (error) {
        // Error talking to server, pass the error on for now
        return data[0](error, null);
    } else if (body && body.error) {
        // This should probably be updated to act differently
        var errObj = new Error("REST error " + body.error);
        errObj.code = 9999;
        if (body.reason) {
            errObj.reason = body.reason;
        }
        return data[0](errObj, null);
    } else {
        if (body.rows) {
            return data[0](null, body.rows);
        } else if (body.results) {
            return data[0](null, body.results);
        } else {
            return data[0](null, body);
        }

    }
}
