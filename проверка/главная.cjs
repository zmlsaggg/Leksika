const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
require('../учебная-машина.js');
const script = fs.readFileSync('главная.js', 'utf8');
const start = script.indexOf('  const образцы = {');
const end = script.indexOf('\n  const список', start);
assert(start >= 0 && end > start, 'Не найдены образцы');
const samples = vm.runInNewContext(script.slice(start, end) + '\nобразцы');
for (const [name, sample] of Object.entries(samples)) {
  const lines = [];
  ЛексикаУчебная.создать(sample.код, {}, line => lines.push(line));
  assert.equal(lines.join('\n'), sample.вывод, name + ': вывод не соответствует программе');
}
console.log('Образцы главной страницы выполнены учебной машиной.');
