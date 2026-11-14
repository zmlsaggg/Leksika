(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const canvas = $('поле-игры'), ctx = canvas.getContext('2d');
  let timer = null, type = '', snake, direction, pending, food, paddle, ball, score, finished;
  function stop() { if (timer !== null) clearInterval(timer); timer = null; }
  function point() {
    const empty = [];
    for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++)
      if (!snake.some(p => p.x === x && p.y === y)) empty.push({x, y});
    return empty.length ? empty[Math.floor(Math.random() * empty.length)] : null;
  }
  function draw() {
    ctx.clearRect(0, 0, 320, 320);
    if (type === 'snake') {
      ctx.fillStyle = '#74e1a6'; for (const p of snake) ctx.fillRect(p.x * 20 + 1, p.y * 20 + 1, 18, 18);
      if (food) { ctx.fillStyle = '#ff7794'; ctx.fillRect(food.x * 20 + 2, food.y * 20 + 2, 16, 16); }
    } else {
      ctx.fillStyle = '#a9caff'; ctx.fillRect(paddle, 300, 60, 8);
      ctx.beginPath(); ctx.arc(ball.x, ball.y, 7, 0, Math.PI * 2); ctx.fill();
    }
    $('счёт-игры').textContent = (finished || 'Счёт') + ': ' + score;
  }
  function tick() {
    if (type === 'snake') {
      direction = pending;
      const head = {x: snake[0].x + direction.x, y: snake[0].y + direction.y};
      const eat = head.x === food.x && head.y === food.y;
      if (head.x < 0 || head.x >= 16 || head.y < 0 || head.y >= 16 || snake.slice(0, eat ? undefined : -1).some(p => p.x === head.x && p.y === head.y)) { finished = 'Игра окончена'; stop(); }
      else {
        snake.unshift(head); if (eat) { score++; food = point(); if (!food) { finished = 'Победа'; stop(); } } else snake.pop();
      }
    } else {
      ball.x += ball.dx; ball.y += ball.dy;
      if (ball.x <= 7 || ball.x >= 313) ball.dx *= -1;
      if (ball.y <= 7) ball.dy = Math.abs(ball.dy);
      if (ball.dy > 0 && ball.y >= 293 && ball.y <= 308 && ball.x >= paddle && ball.x <= paddle + 60) { ball.dy = -Math.abs(ball.dy); score++; }
      if (ball.y > 327) { finished = 'Игра окончена'; stop(); }
    }
    draw();
  }
  function start(game) {
    stop(); type = game; score = 0; finished = '';
    $('игра').hidden = false; $('название-игры').textContent = game === 'snake' ? 'Змейка' : 'Пинг-понг';
    if (game === 'snake') { snake = [{x: 8, y: 8}, {x: 7, y: 8}, {x: 6, y: 8}]; direction = pending = {x: 1, y: 0}; food = point(); }
    else { paddle = 130; ball = {x: 160, y: 130, dx: 3, dy: 3}; }
    draw(); timer = setInterval(tick, game === 'snake' ? 130 : 20);
  }
  function move(key) {
    if (timer === null) return;
    if (type === 'snake') {
      const choices = {влево: {x:-1,y:0}, вправо: {x:1,y:0}, вверх: {x:0,y:-1}, вниз: {x:0,y:1}};
      const next = choices[key]; if (next && next.x !== -direction.x && next.y !== -direction.y) pending = next;
    } else if (key === 'влево') paddle = Math.max(0, paddle - 24);
    else if (key === 'вправо') paddle = Math.min(260, paddle + 24);
    draw();
  }
  $('змейка').addEventListener('click', () => start('snake'));
  $('понг').addEventListener('click', () => start('pong'));
  $('стоп-игры').addEventListener('click', () => { stop(); finished = 'Остановлено'; draw(); });
  document.querySelectorAll('[data-направление]').forEach(button => button.addEventListener('click', () => move(button.dataset.направление)));
  document.addEventListener('keydown', e => {
    const keys = {ArrowLeft:'влево', ArrowRight:'вправо', ArrowUp:'вверх', ArrowDown:'вниз'};
    if (keys[e.key] && timer !== null) { e.preventDefault(); move(keys[e.key]); }
  });
  document.addEventListener('visibilitychange', () => { if (document.hidden && timer !== null) { stop(); finished = 'Остановлено'; draw(); } });
})();
