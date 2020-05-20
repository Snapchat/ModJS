
var keydb = new Object();
keydb.commands = new Object();

keydb.modjs_version = _internal.version();

keydb.call = function(...args)
{
    return _internal.call(...args);
}

keydb.register = function(fn, flags = "write deny-oom random", keyFirst = 0, keyLast = 0, keyStep = 0)
{
    this.commands += fn;
    return _internal.register(fn, flags, keyFirst, keyLast, keyStep);
}

keydb.log = function()
{
    if (arguments.length == 1) {
        _internal.log("notice", arguments[0]);
    } else if (arguments.length == 2) {
        _internal.log(arguments[0], arguments[1]);
    } else {
        _internal.log("warning", "log() called with invalid arguments");
    } 
}

var console = {log: keydb.log}  // alias keydb.log to console.log
var redis = keydb;  // Alias