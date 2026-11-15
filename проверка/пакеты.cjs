const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'leksika-package-'));
try {
  const names = ['windows-x64.zip', 'macos-arm64.tar.gz', 'macos-x64.tar.gz', 'linux-x64.tar.gz', 'android-arm64.tar.gz'];
  const sums = names.map((n,i) => `${String(i+1).repeat(64)}  leksika-native-${n}`).join('\n') + '\n';
  const sumfile = path.join(dir, 'checksums.txt'); fs.writeFileSync(sumfile, sums);
  const run = version => execFileSync('python3', ['packaging/update_templates.py', version, sumfile, dir], {stdio:'pipe'});
  run('1.2.3');
  const scoop = JSON.parse(fs.readFileSync(path.join(dir, 'leksus-core.json'), 'utf8'));
  assert.equal(scoop.version, '1.2.3'); assert.equal(scoop.architecture['64bit'].hash, '1'.repeat(64));
  assert(scoop.architecture['64bit'].url.includes('/v1.2.3/'));
  const formula = fs.readFileSync(path.join(dir, 'leksus-core.rb'), 'utf8');
  assert(formula.includes('version "1.2.3"'));
  for (let i = 2; i <= 4; i++) assert(formula.includes(String(i).repeat(64)));
  assert.throws(() => run('1.2.3-beta.1'));
  fs.writeFileSync(sumfile, sums.replace(/leksika-native-linux-x64.tar.gz/, 'missing.tar.gz'));
  assert.throws(() => run('1.2.3'));
  console.log('Шаблоны пакетов проверены.');
} finally { fs.rmSync(dir, {recursive:true, force:true}); }
