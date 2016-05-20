'use strict';

const addon = require('../build/Release/addon');

addon.send('lo', '00:00:00:00:00:00', Buffer.from('test'));
