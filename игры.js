(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const canvas = $('поле-игры'), ctx = canvas.getContext('2d');
  const цвета = {1: '#74e1a6', 2: '#ff7794', 3: '#a9caff'};
  const конфигурация = {
    snake: {название: 'Змейка', файл: 'программы/змейка.lek', интервал: 130},
    pong: {название: 'Пинг-понг', файл: 'программы/пинг-понг.lek', интервал: 20},
    calculator: {название: 'Калькулятор', файл: 'программы/калькулятор.lek', интервал: 0}
  };
  let timer = null, machine = null, generation = 0, game = null, score = 0, active = false;
  const dialog = $('итог-игры');
  function closeDialog() { if (dialog.open) dialog.close(); }
  function stop() { generation++; if (timer !== null) clearInterval(timer); timer = null; machine = null; active = false; }
  function finish(label, points) {
    if (!game) return;
    score = Number.isFinite(points) ? points : score;
    stop();
    $('счёт-игры').textContent = label + ': ' + score;
    $('итог-заголовок').textContent = label;
    $('итог-счёт').textContent = String(score);
    if (!dialog.open) dialog.showModal();
  }
  function error(err) { stop(); $('счёт-игры').textContent = 'Ошибка игры: ' + err.message; }
  function invoke(name, ...args) { try { if (machine && active) machine.вызвать(name, ...args); } catch (err) { error(err); } }
  const действия = {
    очистить: () => ctx.clearRect(0, 0, 320, 320),
    квадрат: (x, y, size, color) => { ctx.fillStyle = цвета[color]; ctx.fillRect(x, y, size, size); },
    прямоугольник: (x, y, w, h, color) => { ctx.fillStyle = цвета[color]; ctx.fillRect(x, y, w, h); },
    круг: (x, y, r, color) => { ctx.fillStyle = цвета[color]; ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill(); },
    показатьсчёт: points => { score = points; $('счёт-игры').textContent = 'Счёт: ' + score; },
    завершить: (points, won) => finish(won ? 'Победа!' : 'Игра окончена', points),
    задание: (a, b) => { $('пример-калькулятора').textContent = `${a} + ${b} = ?`; $('ответ-калькулятора').value = ''; }
  };
  async function start(kind) {
    closeDialog(); stop(); const current = generation;
    game = kind; score = 0; active = true;
    const config = конфигурация[kind];
    $('игра').hidden = false; $('название-игры').textContent = config.название;
    canvas.hidden = kind === 'calculator'; $('форма-калькулятора').hidden = kind !== 'calculator';
    $('стрелки-игры').hidden = kind === 'calculator';
    $('счёт-игры').textContent = 'Загружаем программу…';
    try {
      const response = await fetch(config.файл);
      if (!response.ok) throw new Error('Не удалось загрузить программу');
      const source = await response.text(); if (current !== generation) return;
      machine = ЛексикаУчебная.создать(source, действия);
      machine.вызвать('начать');
      if (current !== generation || !machine) return;
      if (config.интервал) timer = setInterval(() => invoke('шаг'), config.интервал);
      else $('ответ-калькулятора').focus();
    } catch (err) { if (current === generation) error(err); }
  }
  const keys = {влево: 1, вправо: 2, вверх: 3, вниз: 4};
  $('змейка').addEventListener('click', () => start('snake'));
  $('понг').addEventListener('click', () => start('pong'));
  $('калькулятор').addEventListener('click', () => start('calculator'));
  $('стоп-игры').addEventListener('click', () => { if (active) finish('Игра остановлена', score); });
  $('перезапустить-игру').addEventListener('click', () => start(game));
  $('закрыть-итог').addEventListener('click', closeDialog);
  $('форма-калькулятора').addEventListener('submit', e => {
    e.preventDefault();
    const input = $('ответ-калькулятора');
    if (!active || game !== 'calculator' || input.value.trim() === '' || !input.validity.valid) return;
    invoke('ответ', Number(input.value));
    if (active) input.focus();
  });
  document.querySelectorAll('[data-направление]').forEach(button => button.addEventListener('click', () => invoke('управление', keys[button.dataset.направление])));
  document.addEventListener('keydown', e => {
    const arrows = {ArrowLeft:'влево', ArrowRight:'вправо', ArrowUp:'вверх', ArrowDown:'вниз'};
    if (arrows[e.key] && timer !== null && active) { e.preventDefault(); invoke('управление', keys[arrows[e.key]]); }
  });
  document.addEventListener('visibilitychange', () => { if (document.hidden && active) finish('Игра остановлена', score); });
})();
