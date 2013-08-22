/**
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
 *
 *   (for jsdoc)
 *   @copyright 2013 Couchbase, Inc.
 */

var Memcached = require("memcached");
var Couchbase = require("../lib/couchbase.js");
var util = require("util");

// Globals
var MAX_CLIENTS = 20;
var MAX_OPERATIONS=10000000;
var MODE_SEQUENTIAL = 1;
var CurrentOperations = 0;

var key = "keybase";
var value = "valbase";

for (var i = 0; i < 10; i++) {
    value += "***";
}

/* How many operations are remaining */
var remaining=MAX_OPERATIONS;
var BEGIN_TIME = Date.now();

setInterval(function(arg) {
    // Get time elapsed
    var elapsedTime = (Date.now() - BEGIN_TIME) / 1000;
    var opsPerSecond = CurrentOperations / elapsedTime;
    var s = util.format("Elapsed: %d; Ops=%d [%d]/S",
        elapsedTime,
        CurrentOperations,
        Math.round(opsPerSecond));
    for (var i = 0; i < 20; i++) {
        s+= ' ';
    }
    process.stdout.write(s + "\r");
}, 100, null);


function markOperation(err) {
    if (err) {
        throw new Error("Got err: " + err);
    }
    CurrentOperations++;
    remaining--;
    if (!remaining) {
        console.log("");
        console.log("Done!")
        process.exit(1);
    }
}


function scheduleMemcached(cb) {
    cb.set(key, value, 0, function(err) {
        markOperation(err);

        cb.get(key, function(err, meta) {
            markOperation(err);
            scheduleMemcached(cb);
        });
    });
}

function scheduleCouchbase(cb) {
    cb.set(key, value, {}, function(err, meta) {
        markOperation(err);
        cb.get(key, function(err, meta) {
            markOperation(err);
            scheduleCouchbase(cb);
        })
    })
}


function launchMemcachedClient() {
    var mc = new Memcached("localhost:11311");
    scheduleMemcached(mc);
}

function launchCouchbaseClient() {
    var cbOptions = {
        bucket: "memd",
        connectionTimeout: 500000,
        operationTimeout: 500000,
    };

    var cb = new Couchbase(cbOptions, function(err) {
        if (err) {
            console.log("Got error on connect: " + err);
            process.exit(1);
        } else {
            console.log("Connected!");
        }
    });
    scheduleCouchbase(cb);
}



for (var i = 0; i < MAX_CLIENTS; i++) {
    if (process.argv[2] == "memcached") {
        //console.log("Starting Memcached");
        launchMemcachedClient();
    } else {
        //console.log("Starting Couchbase");
        launchCouchbaseClient();
    }
}

/** OUTPUT:
 mnunberg@csure:~/src/couchnode$ time node memdbench.js memcached
Elapsed: 12.706; Ops=99283 [7814]/S                    
Done!

real	0m12.850s
user	0m10.549s
sys	0m2.368s
mnunberg@csure:~/src/couchnode$ time node memdbench.js 
Elapsed: 4.842; Ops=99850 [20622]/S                    
Done!

real	0m4.915s
user	0m3.356s
sys	0m1.576s
**/
