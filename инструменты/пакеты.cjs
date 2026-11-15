#!/usr/bin/env node
'use strict';

// Только чтение открытого каталога npm. Установка и запуск пакетов — отдельно.
const https = require('node:https');
const { TextDecoder } = require('node:util');
const MAX_BYTES = 1024 * 1024;
const TIMEOUT_MS = 10000;

function безопаснаяСтрока(value, limit = 200) {
  if (typeof value !== 'string') return '';
  // Данные каталога недоверенные: управляющие символы терминалу не передаём.
  return Array.from(value.replace(/[\u0000-\u001f\u007f-\u009f\u202a-\u202e\u2066-\u2069]/g, ' ')).slice(0, limit).join('');
}
function адресПоиска(query) {
  if (typeof query !== 'string' || !query.trim() || query.length > 200 || /[\u0000-\u001f\u007f-\u009f]/.test(query)) {
    throw new Error('Поисковый запрос должен содержать от 1 до 200 символов без управляющих знаков.');
  }
  const url = new URL('https://registry.npmjs.org/-/v1/search');
  url.searchParams.set('text', query.trim());
  url.searchParams.set('size', '10');
  return url;
}
function получить(url, get = https.get) {
  return new Promise((resolve, reject) => {
    let finished = false;
    let timer;
    const fail = error => { if (!finished) { finished = true; clearTimeout(timer); reject(error); } };
    const req = get(url, {headers: {'accept': 'application/json'}}, response => {
      // Редиректы не следуем: данные должны приходить только от заданного реестра.
      if (response.statusCode !== 200) {
        response.resume(); fail(new Error(`Реестр ответил HTTP ${response.statusCode}.`)); return;
      }
      const chunks = []; let size = 0;
      response.on('data', chunk => {
        size += chunk.length;
        if (size > MAX_BYTES) { fail(new Error('Ответ реестра слишком велик.')); req.destroy(); return; }
        chunks.push(chunk);
      });
      response.on('error', fail);
      response.on('aborted', () => fail(new Error('Ответ реестра прерван.')));
      response.on('end', () => {
        if (finished) return;
        try {
          const text = new TextDecoder('utf-8', {fatal: true}).decode(Buffer.concat(chunks));
          const data = JSON.parse(text);
          finished = true; clearTimeout(timer); resolve(data);
        } catch { fail(new Error('Реестр вернул некорректный JSON.')); }
      });
    });
    req.on('error', fail);
    timer = setTimeout(() => { fail(new Error('Время ожидания реестра истекло.')); req.destroy(); }, TIMEOUT_MS);
    // При ошибке синхронного mock-ответа таймер тоже не должен оставаться активным.
    if (finished) clearTimeout(timer);
  });
}
function строкиРезультата(data) {
  if (!data || !Array.isArray(data.objects)) throw new Error('Неожиданный формат ответа реестра.');
  return data.objects.slice(0, 10).map(item => {
    const pkg = item && item.package;
    if (!pkg || typeof pkg.name !== 'string' || typeof pkg.version !== 'string') throw new Error('Неожиданный формат ответа реестра.');
    const name = безопаснаяСтрока(pkg.name, 214), version = безопаснаяСтрока(pkg.version, 100);
    const description = безопаснаяСтрока(pkg.description);
    return `${name} (${version})${description ? ' — ' + description : ''}`;
  });
}
async function main(args) {
  if (args.length === 1 && (args[0] === 'справка' || args[0] === '--help')) {
    console.log('Использование: node инструменты/пакеты.cjs npm поиск "запрос"');
    console.log('Поиск в публичном реестре npm; ничего не устанавливает и не запускает.');
    return 0;
  }
  if (args.length !== 3 || args[0] !== 'npm' || args[1] !== 'поиск') {
    console.error('Укажите: npm поиск "запрос"'); return 2;
  }
  try {
    const lines = строкиРезультата(await получить(адресПоиска(args[2])));
    console.log(lines.length ? lines.join('\n') : 'Пакеты не найдены.');
    return 0;
  } catch (error) {
    console.error('Поиск npm: ' + безопаснаяСтрока(error.message)); return 1;
  }
}
if (require.main === module) main(process.argv.slice(2)).then(code => { process.exitCode = code; });
module.exports = { адресПоиска, безопаснаяСтрока, получить, строкиРезультата };
