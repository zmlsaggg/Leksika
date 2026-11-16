(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const canvas = $('поле-игры'), ctx = canvas.getContext('2d');
  const цвета = {1: '#74e1a6', 2: '#ff7794', 3: '#a9caff', 4: '#815dcc', 5: '#c25378', 6: '#d18d4e', 7: '#5faaa2', 8: '#25273b', 9: '#39313b', 10: '#57506b', 11: '#786782', 12: '#a18aa0', 13: '#ffd966'};
  const конфигурация = {
    snake: {название: 'Змейка', файл: 'программы/змейка.lek', интервал: 130},
    pong: {название: 'Пинг-понг', файл: 'программы/пинг-понг.lek', интервал: 20},
    strike: {название: 'Strike', файл: 'программы/страйк.lek', интервал: 20},
    maze: {название: 'Шутер-лабиринт', файл: 'программы/лабиринт.lek', интервал: 150},
    pacman: {название: 'Пакман', файл: 'программы/пакман.lek', интервал: 160},
    calculator: {название: 'Калькулятор', файл: 'программы/калькулятор.lek', интервал: 0},
    dino: {название: 'Динозаврик', файл: 'программы/динозаврик.lek', интервал: 40},
    mario: {название: 'Мини-платформер', файл: 'программы/мини-марио.lek', интервал: 40}
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
    надпись: (text, x, y, color) => { ctx.fillStyle = цвета[color]; ctx.font = '14px sans-serif'; ctx.fillText(String(text), x, y); },
    табло: (a, b) => { score = a; $('счёт-игры').textContent = `Игрок: ${a} · Компьютер: ${b}`; },
    конецматча: (a, b) => { finish(a > b ? 'Вы победили!' : 'Победил компьютер', a); },
    показатьсчёт: points => { score = points; $('счёт-игры').textContent = 'Счёт: ' + score; },
    завершить: (points, won) => finish(won ? 'Победа!' : 'Игра окончена', points),
    задание: (a, b) => { $('пример-калькулятора').textContent = `${a} + ${b} = ?`; $('ответ-калькулятора').value = ''; }
  };
  async function start(kind) {
    closeDialog(); stop(); const current = generation;
    game = kind; score = 0; active = true;
    const config = конфигурация[kind];
    const кнопка = document.querySelector(`[data-игра="${kind}"]`);
    if (кнопка) кнопка.closest('.карточка-игры').after($('игра'));
    $('игра').hidden = false; $('название-игры').textContent = config.название;
    canvas.hidden = kind === 'calculator'; $('форма-калькулятора').hidden = kind !== 'calculator';
    $('стрелки-игры').hidden = kind === 'calculator';
    $('выстрел').hidden = !['maze', 'dino', 'mario'].includes(kind);
    $('выстрел').textContent = kind === 'maze' ? 'Выстрел' : 'Прыжок';
    $('подсказка-игры').textContent = kind === 'pong' ? 'Вы — нижняя ракетка, компьютер — верхняя. Стрелки или кнопки. Первый пропуск решает матч.' : kind === 'maze' ? 'Стрелки: идти и поворачивать. Пробел или кнопка — выстрел. Победите всех врагов.' : kind === 'pacman' ? 'Соберите все точки, избегая призраков. Большие точки позволяют временно их съесть.' : kind === 'dino' ? 'Вверх, пробел или кнопка: прыжок через кактус.' : kind === 'mario' ? 'Влево и вправо выбирают направление; вверх, пробел или кнопка — прыжок. Доберитесь до флага.' : 'Управление стрелками или кнопками.';
    $('счёт-игры').textContent = 'Загружаем программу…';
    try {
      const response = await fetch(config.файл);
      if (!response.ok) throw new Error('Не удалось загрузить программу');
      const source = await response.text(); if (current !== generation) return;
      machine = ЛексикаУчебная.создать(source, действия);
      machine.вызвать('начать');
      if (current !== generation || !machine) return;
      if (config.интервал) timer = setInterval(() => invoke('шаг'), config.интервал);
      else if (kind === 'calculator') $('ответ-калькулятора').focus();
    } catch (err) { if (current === generation) error(err); }
  }
  const keys = {влево: 1, вправо: 2, вверх: 3, вниз: 4, огонь: 7};
  const кнопки = {змейка:'snake', понг:'pong', страйк:'strike', пакман:'pacman', лабиринт:'maze', калькулятор:'calculator', динозаврик:'dino', марио:'mario'};
  for (const [id, kind] of Object.entries(кнопки)) { const button = $(id); button.dataset.игра = kind; button.addEventListener('click', () => start(kind)); }
  document.querySelectorAll('[data-фильтр]').forEach(button => button.addEventListener('click', () => {
    const filter = button.dataset.фильтр;
    document.querySelectorAll('[data-фильтр]').forEach(b => b.setAttribute('aria-pressed', String(b === button)));
    document.querySelectorAll('[data-категория]').forEach(card => { card.hidden = filter !== 'все' && card.dataset.категория !== filter; });
    if (filter === 'пользовательские') { stop(); $('игра').hidden = true; closeDialog(); }
    document.dispatchEvent(new CustomEvent('фильтр-каталога', {detail:filter}));
  }));
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
    const key = arrows[e.key] || (['maze', 'dino', 'mario'].includes(game) && e.key === ' ' ? 'огонь' : null);
    if (key && active && game !== 'calculator') { e.preventDefault(); invoke('управление', keys[key]); }
  });
  document.addEventListener('visibilitychange', () => { if (document.hidden && active) finish('Игра остановлена', score); });
})();
