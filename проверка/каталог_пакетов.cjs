'use strict';
const assert = require('node:assert/strict');
const {EventEmitter} = require('node:events');
const {spawnSync} = require('node:child_process');
const path = require('node:path');
const {адресПоиска, безопаснаяСтрока, получить, строкиРезультата} = require('../инструменты/пакеты.cjs');

function mock(status, chunks, options = {}) {
  return (url, config, callback) => {
    const req = new EventEmitter(); req.destroy = () => {};
    process.nextTick(() => {
      const res = new EventEmitter(); res.statusCode = status; res.resume = () => {};
      callback(res);
      for (const chunk of chunks) res.emit('data', Buffer.from(chunk));
      if (options.aborted) res.emit('aborted'); else res.emit('end');
    });
    return req;
  };
}
(async () => {
  const url = адресПоиска('@scope/a & тест');
  assert.equal(url.hostname, 'registry.npmjs.org');
  assert.equal(url.searchParams.get('text'), '@scope/a & тест');
  assert.throws(() => адресПоиска('')); assert.throws(() => адресПоиска('x\n'));
  assert.equal(безопаснаяСтрока('x\x1b[2J\nabc\u202e'), 'x [2J abc ');
  const data = await получить(url, mock(200, ['{"objects":[{"package":{"name":"test","version":"1.0.0","description":"описание"}}]}']));
  assert.deepEqual(строкиРезультата(data), ['test (1.0.0) — описание']);
  assert.throws(() => строкиРезультата({objects:[{}]}));
  assert.equal(строкиРезультата({objects: Array(20).fill({package:{name:'x',version:'1'}})}).length, 10);
  await assert.rejects(получить(url, mock(302, [])), /HTTP 302/);
  await assert.rejects(получить(url, mock(200, ['{'])), /JSON/);
  await assert.rejects(получить(url, mock(200, ['x'.repeat(1024 * 1024 + 1)])), /велик/);
  await assert.rejects(получить(url, mock(200, [], {aborted:true})), /прерван/);
  const cli = path.resolve(__dirname, '../инструменты/пакеты.cjs');
  assert.equal(spawnSync(process.execPath, [cli, 'npm', 'неизвестно'], {encoding:'utf8'}).status, 2);
  console.log('Поиск пакетов npm проверен без сети.');
})().catch(error => { console.error(error); process.exitCode = 1; });
