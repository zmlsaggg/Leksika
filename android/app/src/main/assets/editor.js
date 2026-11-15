(() => {
  'use strict';
  const code = document.getElementById('code'), output = document.getElementById('output');
  const run = document.getElementById('run'), stop = document.getElementById('stop'), status = document.getElementById('status');
  let worker = null, timer = null;
  function finish(message) {
    if (worker) worker.terminate();
    worker = null; clearTimeout(timer); timer = null;
    run.disabled = false; stop.disabled = true; status.textContent = message;
  }
  run.addEventListener('click', () => {
    if (worker) return;
    output.textContent = '';
    if (code.value.length > 20000) { status.textContent = 'Код слишком длинный (максимум 20 000 символов)'; return; }
    try {
      worker = new Worker('worker.js');
      run.disabled = true; stop.disabled = false; status.textContent = 'Выполняется…';
      worker.onmessage = event => { output.textContent = event.data.output; finish(event.data.error ? 'Ошибка: ' + event.data.error : 'Выполнено'); };
      worker.onerror = () => finish('Не удалось запустить учебную машину');
      timer = setTimeout(() => finish('Превышено время выполнения'), 5000);
      worker.postMessage(code.value);
    } catch (_) { finish('Не удалось запустить учебную машину'); }
  });
  stop.addEventListener('click', () => finish('Остановлено'));
})();
