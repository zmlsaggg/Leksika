'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const adapter = path.resolve(__dirname, '../инструменты/узел.cjs');
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'leksika node '));
try {
  const pkg = path.join(dir, 'node_modules', 'local-example');
  fs.mkdirSync(pkg, {recursive: true});
  fs.writeFileSync(path.join(pkg, 'index.js'), 'module.exports = "пакет";');
  const file = path.join(dir, 'пример.js');
  fs.writeFileSync(file, 'console.log(require("local-example"), process.argv[2]);');
  const run = (...args) => spawnSync(process.execPath, [adapter, ...args], {cwd:dir, encoding:'utf8'});
  let result = run('запустить', file, 'значение с пробелами');
  assert.equal(result.status, 0, result.stderr);
  assert.equal(result.stdout.trim(), 'пакет значение с пробелами');
  result = run('запустить', '--version'); assert.equal(result.status, 2);
  result = run('запустить', path.join(dir, 'нет.js')); assert.notEqual(result.status, 0);
  result = run('неизвестная'); assert.equal(result.status, 2);
  fs.writeFileSync(file, 'process.exit(7);');
  result = run('запустить', file); assert.equal(result.status, 7);
  console.log('Необязательный запуск Node.js проверен.');
} finally { fs.rmSync(dir, {recursive:true, force:true}); }
