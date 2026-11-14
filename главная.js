(() => {
  'use strict';
  // Это заранее проверенные образцы, а не запуск кода на главной странице.
  const образцы = {
    сумма: { код: 'пусть сумма = 5;\nсумма += 10;\nсумма += 2;\nсказать(сумма);', вывод: '17' },
    цикл: { код: 'пусть шаг = 1;\nпока (шаг <= 3) {\n    сказать(шаг);\n    шаг++;\n}', вывод: '1\n2\n3' },
    правило: { код: 'правило сумма(а, б) {\n    вернуть(а + б);\n}\nсказать(сумма(12, 30));', вывод: '42' }
  };
  const список = document.querySelector('.переключатель-образцов');
  const код = document.getElementById('образец-код');
  const вывод = document.getElementById('образец-вывод');
  if (!список || !код || !вывод) return;
  список.hidden = false;
  список.addEventListener('click', event => {
    const button = event.target.closest('button[data-образец]');
    if (!button || !список.contains(button)) return;
    const пример = образцы[button.dataset.образец];
    if (!пример) return;
    код.textContent = пример.код;
    вывод.textContent = пример.вывод;
    список.querySelectorAll('button').forEach(item => item.setAttribute('aria-pressed', String(item === button)));
  });
})();
