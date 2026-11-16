'use strict';
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const machine = fs.readFileSync('учебная-машина.js','utf8');
const worker = fs.readFileSync('android/app/src/main/assets/worker.js','utf8');
let result;
const context = vm.createContext({importScripts: name => { assert.equal(name,'machine.js'); vm.runInContext(machine,context); }, postMessage: value => { result = value; }});
vm.runInContext(worker, context);
context.onmessage({data:'пусть сумма = 5; сумма += 10; сумма += 2; сказать(сумма);'});
assert.equal(result.output,'17'); assert.equal(result.error,'');
context.onmessage({data:'сказать(необъявленное);'});
assert.notEqual(result.error,'');
context.onmessage({data:'пока (да) { сказать(1); }'});
assert.notEqual(result.error,'');
const execute = source => { context.onmessage({data:source}); return result; };
for (const verb of ['Сказал','сказала','сказало','сказали','скажу','скажет','скажите','что']) {
  assert.equal(execute(`${verb} 42.`).output, '42', verb);
  assert.equal(result.error, '', verb);
}
assert.equal(execute('пусть сумма равно пяти. положить десять в сумму. прибавить два к сумме. отнять один от суммы. Сказал сумму.').output, '11');
assert.equal(execute('почему да то речь Ошибка конец речи. что 42.').output, '42');
assert.equal(execute('почему нет то речь Причина конец речи.').error, 'Причина');
assert.notEqual(execute('почему нет речь Без маркера конец речи.').error, '');
assert.notEqual(execute('почему нет то 42.').error, '');
assert.equal(execute('когда да тогда что 42. конец когда.').output, '42');
assert.equal(execute('пусть элементы равно перечень(1, 2). где элемент из элементы повторять что элемент. конец где.').output, '1\n2');
assert.equal(execute('пусть текст равно речь Ёж 🐈 конец речи. что длина текст.').output, '4');
assert.equal(execute('пусть кошка равно 1. пусть кошку равно 2. что кошке.').error.includes('неоднозначна'), true);
assert.notEqual(execute('прибавить один к неизвестному.').error, '');
assert.equal(execute('сказать речь Сказал что почему конец речи.').output, 'Сказал что почему');
// Примеры живут в editor.js: index.html остаётся статичной разметкой,
// а стартовый код подставляется скриптом. Раньше пример был прямо в
// textarea, и проверка ловила только его; теперь проверяем все восемь.
const html = fs.readFileSync('android/app/src/main/assets/index.html', 'utf8');
const editor = fs.readFileSync('android/app/src/main/assets/editor.js', 'utf8');
assert(!/<textarea id="code"[^>]*>[^<]/.test(html), 'код примера не должен дублироваться в разметке');
for (const имя of ['знакомство','условие','выбор','циклы','правило','перечень','таблица','фибоначчи']) {
  assert(html.includes('data-пример="' + имя + '"'), 'в разметке нет кнопки примера ' + имя);
  assert(editor.includes(имя + ':'), 'в editor.js нет примера ' + имя);
}
/* Запускаем каждый пример из editor.js так, как это сделает кнопка:
   вырезаем литералы и подставляем в учебную машину. */
const блок = editor.slice(editor.indexOf('const образцы = {'), editor.indexOf('\n  };', editor.indexOf('const образцы = {')) + 4);
const образцы = vm.runInNewContext(блок + '\nобразцы;', {});
assert.equal(Object.keys(образцы).length, 8, 'ожидалось восемь примеров');
for (const [имя, код] of Object.entries(образцы)) {
  const r = execute(код);
  assert.equal(r.error, '', 'пример «' + имя + '» падает: ' + r.error);
  assert.notEqual(r.output, '', 'пример «' + имя + '» ничего не выводит');
}
assert.equal(execute(образцы.знакомство).output, 'Сумма сейчас 22.');
const manifest = fs.readFileSync('android/app/src/main/AndroidManifest.xml','utf8');
assert(manifest.includes('@drawable/launcher'));
assert(fs.statSync('android/app/src/main/res/drawable-nodpi/launcher.png').size > 0);
assert(!manifest.includes('android.permission.INTERNET'));
console.log('Android: учебная машина и ограничение сети проверены.');
