'use strict';

const addon = require('../build/Release/addon');

addon.listen('lo', function (buffer) {
    console.log('captured package of length', buffer.length);
    console.log(buffer.toString('UTF8', 14));
});

addon.send('lo', '00:00:00:00:00:00', Buffer.from('test'));
