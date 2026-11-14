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
      const run = async (name) => {
        await page.selectOption('#пример', name);
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
      const first = await run('яблоко');
      assert(await page.locator('#остановить-анимацию').isVisible());
      await pause(1400);
      assert.notEqual(await page.locator('#вывод').innerText(), first, 'Кадры анимации не меняются');
      await page.click('#остановить-анимацию');
      const stopped = await page.locator('#вывод').innerText();
      await pause(300); assert.equal(await page.locator('#вывод').innerText(), stopped);
      await page.selectOption('#пример', 'начало');
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
      await page.close();
      console.log(`Песочница проверена: ${viewport.width}x${viewport.height}`);
    }
  } finally { if (browser) await browser.close(); server.kill(); }
})().catch(error => { console.error(error); console.error(`::error::${String(error).replace(/[\r\n]/g, ' ')}`); process.exitCode = 1; });
