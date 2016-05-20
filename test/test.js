'use strict';

const addon = require('../build/Release/addon');

addon.send('eth0', '00:00:00:00:00:00', Buffer.from('test'));
