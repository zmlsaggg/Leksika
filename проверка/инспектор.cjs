'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const cli = path.resolve(__dirname, '../инструменты/инспектор.cjs');
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'leksika inspector '));
try {
  const source = path.join(dir,'заметка.txt'), out = path.join(dir,'новая.txt');
  fs.writeFileSync(source, '\ufeffСписок покупок\r\nМолоко\r\n');
  const run = (...args) => spawnSync(process.execPath, [cli,...args], {encoding:'utf8'});
  assert.equal(run(source).stdout, '\ufeffСписок покупок\r\nМолоко\r\n');
  let r = run(source,'запись','Список покупок','Че купить',out); assert.equal(r.status,0,r.stderr);
  assert.equal(fs.readFileSync(out,'utf8'), '\ufeffЧе купить\r\nМолоко\r\n');
  assert.match(fs.readFileSync(source,'utf8'), /Список покупок/);
  assert.equal(run(source,'запись','Молоко','Хлеб',out).status,1);
  assert.equal(run(source,'запись','нет','x',path.join(dir,'нет')).status,1);
  fs.writeFileSync(source,'ааа'); assert.equal(run(source,'запись','аа','x',path.join(dir,'неоднозначно')).status,1);
  fs.writeFileSync(source,Buffer.from([0xff])); assert.equal(run(source).status,1);
  fs.writeFileSync(source,Buffer.from([0])); assert.equal(run(source).status,1);
  console.log('Инспектор проверен.');
} finally { fs.rmSync(dir,{recursive:true,force:true}); }
