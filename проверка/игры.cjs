const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
require('../учебная-машина.js');
const source = name => fs.readFileSync(path.join(__dirname, '..', 'программы', name + '.lek'), 'utf8');
const random = Math.random;
try {
  Math.random = () => 0;
  let score = -1, outcome = null, head = null;
  const host = {очистить(){ head = null; }, квадрат(x,y,size,color){ if (color === 1 && !head) head = [x,y]; },
    показатьсчёт(n){score=n;}, завершить(n,won){outcome={n,won};}};
  const snake = ЛексикаУчебная.создать(source('змейка'),host);
  snake.вызвать('начать'); assert.equal(score, 0); assert.deepEqual(head,[161,161]);
  snake.вызвать('управление',1); snake.вызвать('шаг'); assert.deepEqual(head,[181,161], 'нельзя развернуться в хвост');
  snake.вызвать('управление',3); for(let i=0;i<8;i++) snake.вызвать('шаг');
  snake.вызвать('управление',1); for(let i=0;i<9;i++) snake.вызвать('шаг');
  assert.equal(score,1, 'змейка съела еду');
  snake.вызвать('шаг'); assert.deepEqual(outcome,{n:1,won:false}, 'столкновение со стеной');
  let rect, ball, pongScore = -1, end = null;
  const pong = ЛексикаУчебная.создать(source('пинг-понг'), {очистить(){}, прямоугольник(x){rect=x;}, круг(x,y){ball=[x,y];}, показатьсчёт(n){pongScore=n;}, завершить(n,won){end={n,won};}});
  pong.вызвать('начать');
  for(let i=0;i<200 && pongScore===0 && !end;i++) {
    if(ball[0] < rect + 20) pong.вызвать('управление',1);
    if(ball[0] > rect + 40) pong.вызвать('управление',2);
    pong.вызвать('шаг');
  }
  assert.equal(pongScore,1,'мяч отбит');
  const apple = ЛексикаУчебная.создать(source('bad-apple'));
  assert.equal(apple.вызвать('номеркадра',1.5),45);
  assert.equal(apple.вызвать('завершено',6571,6572),false);
  assert.equal(apple.вызвать('завершено',6572,6572),true);
  assert.throws(() => ЛексикаУчебная.создать('правило шаг() { пока (истина) { сказать(1); } }').вызвать('шаг'), /предел/);
  console.log('Программы на Лексике проверены.');
} finally { Math.random = random; }
