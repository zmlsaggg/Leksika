(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const canvas = $('поле-игры'), ctx = canvas.getContext('2d');
  const цвета = {1: '#74e1a6', 2: '#ff7794', 3: '#a9caff'};
  let timer = null, machine = null, generation = 0;
  function stop() { generation++; if (timer !== null) clearInterval(timer); timer = null; machine = null; }
  function error(err) { stop(); $('счёт-игры').textContent = 'Ошибка игры: ' + err.message; }
  function invoke(name, ...args) { try { if (machine) machine.вызвать(name, ...args); } catch (err) { error(err); } }
  const действия = {
    очистить: () => ctx.clearRect(0, 0, 320, 320),
    квадрат: (x, y, size, color) => { ctx.fillStyle = цвета[color]; ctx.fillRect(x, y, size, size); },
    прямоугольник: (x, y, w, h, color) => { ctx.fillStyle = цвета[color]; ctx.fillRect(x, y, w, h); },
    круг: (x, y, r, color) => { ctx.fillStyle = цвета[color]; ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill(); },
    показатьсчёт: score => { $('счёт-игры').textContent = 'Счёт: ' + score; },
    завершить: (score, won) => { if (timer !== null) clearInterval(timer); timer = null; machine = null; $('счёт-игры').textContent = (won ? 'Победа' : 'Игра окончена') + ': ' + score; }
  };
  async function start(game) {
    stop(); const current = generation;
    $('игра').hidden = false; $('название-игры').textContent = game === 'snake' ? 'Змейка' : 'Пинг-понг';
    $('счёт-игры').textContent = 'Загружаем программу…';
    try {
      const response = await fetch(game === 'snake' ? 'программы/змейка.lek' : 'программы/пинг-понг.lek');
      if (!response.ok) throw new Error('Не удалось загрузить программу');
      const source = await response.text(); if (current !== generation) return;
      machine = ЛексикаУчебная.создать(source, действия);
      machine.вызвать('начать');
      timer = setInterval(() => invoke('шаг'), game === 'snake' ? 130 : 20);
    } catch (err) { if (current === generation) error(err); }
  }
  const keys = {влево: 1, вправо: 2, вверх: 3, вниз: 4};
  $('змейка').addEventListener('click', () => start('snake'));
  $('понг').addEventListener('click', () => start('pong'));
  $('стоп-игры').addEventListener('click', () => { stop(); $('счёт-игры').textContent = 'Остановлено'; });
  document.querySelectorAll('[data-направление]').forEach(button => button.addEventListener('click', () => invoke('управление', keys[button.dataset.направление])));
  document.addEventListener('keydown', e => {
    const arrows = {ArrowLeft:'влево', ArrowRight:'вправо', ArrowUp:'вверх', ArrowDown:'вниз'};
    if (arrows[e.key] && timer !== null) { e.preventDefault(); invoke('управление', keys[arrows[e.key]]); }
  });
  document.addEventListener('visibilitychange', () => { if (document.hidden && timer !== null) { stop(); $('счёт-игры').textContent = 'Остановлено'; } });
})();
