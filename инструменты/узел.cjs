#!/usr/bin/env node
'use strict';

// Явный запуск внешнего Node.js: это не часть VM и не песочница Лексики.
const { spawnSync } = require('node:child_process');
const { constants } = require('node:os');
const [команда, файл, ...доводы] = process.argv.slice(2);

if (!команда || команда === 'справка' || команда === '--help') {
  console.log('Использование: node инструменты/узел.cjs запустить файл.js [аргументы...]');
  console.log('Запускает файл во внешнем Node.js; npm-пакеты устанавливаются отдельно.');
  process.exit(команда ? 0 : 2);
}
if (команда !== 'запустить' || !файл || файл.startsWith('-')) {
  console.error('Укажите: запустить файл.js [аргументы...]');
  process.exit(2);
}
// Без shell: имя файла и аргументы не превращаются в команды оболочки.
// -- не даёт Node.js трактовать имя файла как свой флаг.
const результат = spawnSync(process.execPath, ['--', файл, ...доводы], { stdio: 'inherit' });
if (результат.error) {
  console.error('Не удалось запустить Node.js:', результат.error.message);
  process.exit(1);
}
if (результат.signal) {
  console.error('Node.js завершён сигналом', результат.signal);
  process.exit(128 + (constants.signals[результат.signal] || 0));
}
process.exit(результат.status === null ? 1 : результат.status);
