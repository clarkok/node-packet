'use strict';

let addon = require('./build/Release/addon.node');

exports.send = function (iface, content) {
    return addon.send(iface + '', Buffer.from(content));
}

exports.listen = function (iface, callback) {
    return addon.listen(iface + '', callback);
}
