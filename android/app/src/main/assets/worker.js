'use strict';
importScripts('machine.js');
onmessage = event => {
  let output = '', error = '';
  try {
    ЛексикаУчебная.создать(event.data, {}, line => {
      const text = String(line);
      if (output.length + text.length > 50000) throw new Error('Превышен предел вывода');
      output += (output ? '\n' : '') + text;
    });
  } catch (e) { error = String(e && e.message || 'Ошибка программы').slice(0, 500); }
  postMessage({output, error});
};
