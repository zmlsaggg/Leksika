#!/usr/bin/env node
'use strict';
// Запись только в новый файл: исходник не перезаписывается и не удаляется.
const fs = require('node:fs');
const {TextDecoder} = require('node:util');
const LIMIT = 4 * 1024 * 1024;
function прочитать(file) {
  const flags = fs.constants.O_RDONLY | (fs.constants.O_NOFOLLOW || 0);
  const fd = fs.openSync(file, flags);
  try {
    const info = fs.fstatSync(fd);
    if (!info.isFile() || info.size > LIMIT) throw new Error('Нужен обычный файл размером не более 4 МиБ.');
    // Чтение ограничено даже если файл растёт после fstat.
    const buffer = Buffer.alloc(LIMIT + 1); let used = 0, n;
    while (used < buffer.length && (n = fs.readSync(fd, buffer, used, buffer.length - used, null)) > 0) used += n;
    if (used > LIMIT) throw new Error('Файл больше 4 МиБ.');
    if (buffer.subarray(0,used).includes(0)) throw new Error('Файл содержит нулевой байт и не похож на текст.');
    try { return new TextDecoder('utf-8', {fatal:true, ignoreBOM:true}).decode(buffer.subarray(0,used)); }
    catch { throw new Error('Файл должен быть текстом UTF-8.'); }
  } finally { fs.closeSync(fd); }
}
function заменить(text, before, after) {
  if (!before) throw new Error('Искомый текст не может быть пустым.');
  const at = text.indexOf(before);
  if (at < 0) throw new Error('Искомый текст не найден.');
  if (text.indexOf(before, at + 1) >= 0) throw new Error('Найдено несколько совпадений; уточните текст.');
  return text.slice(0, at) + after + text.slice(at + before.length);
}
function main(args) {
  if (args.length === 1 && ['справка','--help'].includes(args[0])) {
    console.log('инспектор файл — показать UTF-8 текст');
    console.log('инспектор файл запись "старый текст" "новый текст" новый-файл — записать изменённую копию');
    return 0;
  }
  if (!(args.length === 1 || (args.length === 5 && args[1] === 'запись'))) {
    console.error('Укажите файл или: файл запись "старый текст" "новый текст" новый-файл'); return 2;
  }
  try {
    const text = прочитать(args[0]);
    if (args.length === 1) {
      // При выводе в терминал убираем управляющие последовательности и bidi-коды.
      const shown = process.stdout.isTTY ? text.replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f-\u009f\u202a-\u202e\u2066-\u2069]/g, '�') : text;
      process.stdout.write(shown);
    }
    else {
      const result = заменить(text, args[2], args[3]);
      if (Buffer.byteLength(result, 'utf8') > LIMIT) throw new Error('Результат больше 4 МиБ.');
      // wx отказывает, если назначение уже существует, включая символическую ссылку.
      fs.writeFileSync(args[4], result, {encoding:'utf8', flag:'wx', mode:0o600});
      console.log('Изменённая копия записана: ' + args[4].replace(/[\x00-\x1f\x7f-\x9f]/g, ' '));
    }
    return 0;
  } catch (error) { console.error('Инспектор: ' + error.message.replace(/[\x00-\x1f\x7f-\x9f]/g, ' ')); return 1; }
}
if (require.main === module) process.exitCode = main(process.argv.slice(2));
module.exports = {прочитать, заменить};
