'use strict';

let addon = require('./build/Release/addon.node');

exports.send = function (iface, mac, content) {
    return addon.send(iface + '', mac + '', Buffer.from(content));
}

exports.listen = function (iface, callback) {
    return addon.listen(iface + '', callback);
}
