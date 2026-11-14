const { chromium } = require('playwright-core');
const { spawn } = require('node:child_process');
const { setTimeout: pause } = require('node:timers/promises');
const assert = require('node:assert/strict');

(async () => {
  const server = spawn('python3', ['-m', 'http.server', '8766', '--bind', '127.0.0.1'], { stdio: 'ignore' });
  let browser;
  try {
    browser = await chromium.launch({ executablePath: process.env.CHROME_PATH || '/usr/bin/google-chrome', args: ['--no-sandbox'] });
    for (const viewport of [{width: 1280, height: 800}, {width: 390, height: 844}]) {
      const page = await browser.newPage({ viewport });
      const requests = [];
      await page.route('**/*', route => {
        const url = route.request().url(); requests.push(url);
        return url.startsWith('http://127.0.0.1:8766/') ? route.continue() : route.abort();
      });
      for (let i = 0; ; i++) {
        try { await page.goto('http://127.0.0.1:8766/playground.html', { waitUntil: 'load' }); break; }
        catch (error) { if (i === 30) throw error; await pause(100); }
      }
      assert.equal(await page.locator('.доп-панель').evaluate(el => el.open), false);
      assert.equal(await page.locator('#ии-запрос').isVisible(), false);
      assert(!requests.some(url => decodeURIComponent(url).includes('разговорные_слова.json')), 'Профили загружены без запроса');
      assert(!requests.some(url => url.includes('huggingface') || url.includes('transformers')), 'Модель загружена без запроса');
      assert(await page.locator('.ниже-редактора').evaluate((el) => el.offsetTop) > await page.locator('.карточка').evaluate((el) => el.offsetTop));
      const select = name => page.locator(`button[data-пример="${name}"]`).click();
      assert.equal(await page.locator('#примеры button').count(), 10);
      if (viewport.width > 850) assert(await page.locator('.страница').evaluate(el => el.getBoundingClientRect().width > 1120), 'Редактор на ПК недостаточно широк');
      const run = async (name) => {
        await select(name);
        assert.equal(await page.locator(`button[data-пример="${name}"]`).getAttribute('aria-pressed'), 'true');
        await page.click('#запустить');
        assert.notEqual(await page.locator('#состояние').getAttribute('data-вид'), 'ошибка', `${name}: ${await page.locator('#состояние').innerText()}`);
        return await page.locator('#вывод').innerText();
      };
      for (const name of ['начало','условие','цикл','правило','список']) await run(name);
      const bottles = await run('бутылки');
      assert(bottles.startsWith('99 bottles of beer'));
      assert.match(bottles, /1 bottle of beer on the wall/);
      assert.match(bottles, /no more bottles of beer on the wall/);
      assert.equal(bottles.split('\n').length, 200);
      const math = await run('арифметика');
      assert.match(math, /Выражение: 26/); assert.match(math, /Остаток: 1/); assert.match(math, /Гипотенуза: 5/);
      assert.equal((await run('фибоначчи')).split('\n').slice(0, 7).join(','), '0,1,1,2,3,5,8');
      assert.equal((await run('простые')).split('\n').join(','), '2,3,5,7,11,13,17,19,23,29');
      await run('яблоко');
      assert(await page.locator('#остановить-анимацию').isVisible());
      await page.waitForFunction(() => /кадр \d+ из 6572/.test(document.getElementById('состояние').textContent), null, {timeout: 30000});
      if (viewport.width > 850) assert(await page.locator('#вывод').evaluate(el => el.scrollWidth <= el.clientWidth + 1 && el.scrollHeight <= el.clientHeight + 1), 'Кадр не помещается в вывод');
      const status = await page.locator('#состояние').innerText();
      await pause(700);
      assert.notEqual(await page.locator('#состояние').innerText(), status, 'Кадры анимации не меняются');
      await page.click('#остановить-анимацию');
      const stopped = await page.locator('#вывод').innerText();
      await pause(300); assert.equal(await page.locator('#вывод').innerText(), stopped);
      await select('начало');
      assert.equal(await page.locator('#остановить-анимацию').isVisible(), false);
      await select('яблоко');
      await page.locator('#код').fill('сказать(42);');
      await page.click('#запустить');
      assert.equal(await page.locator('#вывод').innerText(), '42');
      assert.equal(await page.locator('#остановить-анимацию').isVisible(), false);
      await page.locator('.доп-панель > summary').click();
      assert(await page.locator('#ии-запрос').isVisible());
      await page.locator('.профили-языка > summary').click();
      await page.waitForFunction(() => !document.getElementById('профиль-зумер').disabled);
      assert(requests.some(url => decodeURIComponent(url).includes('разговорные_слова.json')));
      assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), 'Горизонтальная прокрутка');
      await page.goto('http://127.0.0.1:8766/', {waitUntil: 'load'});
      assert.equal(await page.locator('.быстрый-старт > div').count(), 3);
      assert(await page.locator('header img').evaluate(el => el.complete && el.naturalWidth > 0));
      await page.goto('http://127.0.0.1:8766/игры.html', {waitUntil: 'load'});
      await page.click('#змейка');
      assert(await page.locator('#поле-игры').isVisible());
      await page.waitForFunction(() => /Счёт: 0/.test(document.getElementById('счёт-игры').textContent));
      await page.waitForSelector('#итог-игры[open]', {timeout: 7000});
      assert.equal(await page.locator('#итог-счёт').innerText(), '0');
      await page.click('#перезапустить-игру');
      await page.waitForFunction(() => /Счёт: 0/.test(document.getElementById('счёт-игры').textContent));
      await page.click('#стоп-игры');
      assert(await page.locator('#итог-игры').isVisible());
      assert.equal(await page.locator('#итог-счёт').innerText(), '0');
      await page.click('#закрыть-итог');
      await page.click('#понг');
      await page.waitForFunction(() => /Счёт: 0/.test(document.getElementById('счёт-игры').textContent));
      assert.match(await page.locator('#название-игры').innerText(), /Пинг-понг/);
      await page.click('#стоп-игры');
      assert.equal(await page.locator('#итог-счёт').innerText(), '0');
      await page.click('#перезапустить-игру');
      await page.waitForFunction(() => /Счёт: 0/.test(document.getElementById('счёт-игры').textContent));
      await page.click('#стоп-игры'); await page.click('#закрыть-итог');
      await page.click('#калькулятор');
      await page.waitForFunction(() => /\d+ \+ \d+/.test(document.getElementById('пример-калькулятора').textContent));
      const task = await page.locator('#пример-калькулятора').innerText();
      const parts = task.match(/(\d+) \+ (\d+)/);
      await page.locator('#ответ-калькулятора').fill(String(Number(parts[1]) + Number(parts[2])));
      await page.locator('#форма-калькулятора button').click();
      assert.equal(await page.locator('#счёт-игры').innerText(), 'Счёт: 1');
      await page.locator('#ответ-калькулятора').fill('-100');
      await page.locator('#форма-калькулятора button').click();
      assert.equal(await page.locator('#итог-счёт').innerText(), '1');
      await page.click('#перезапустить-игру');
      await page.waitForFunction(() => /Счёт: 0/.test(document.getElementById('счёт-игры').textContent));
      await page.click('#стоп-игры'); await page.click('#закрыть-итог');
      await page.goto('http://127.0.0.1:8766/форум.html', {waitUntil: 'load'});
      assert(await page.locator('#форма-публикации').isVisible());
      assert(await page.locator('#авторизация').isVisible());
      await page.close();
      console.log(`Песочница проверена: ${viewport.width}x${viewport.height}`);
    }
  } finally { if (browser) await browser.close(); server.kill(); }
})().catch(error => { console.error(error); console.error(`::error::${String(error).replace(/[\r\n]/g, ' ')}`); process.exitCode = 1; });
