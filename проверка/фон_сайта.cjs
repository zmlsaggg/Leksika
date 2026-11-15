// Проверка именно отображения кадров в браузере, а не только структуры GIF.
const { chromium } = require('playwright-core');
const { spawn } = require('node:child_process');
const { setTimeout: pause } = require('node:timers/promises');
const assert = require('node:assert/strict');

(async () => {
  const server = spawn('python3', ['-m', 'http.server', '8765', '--bind', '127.0.0.1'], { stdio: 'ignore' });
  let browser;
  try {
    browser = await chromium.launch({ executablePath: process.env.CHROME_PATH || '/usr/bin/google-chrome', args: ['--no-sandbox'] });
    for (const viewport of [{ width: 1200, height: 800 }, { width: 390, height: 844 }]) {
      const page = await browser.newPage({ viewport, reducedMotion: 'no-preference' });
      await page.route('**/*', route => route.request().url().startsWith('http://127.0.0.1:8765/') ? route.continue() : route.abort());
      let loaded = false;
      for (let i = 0; i < 30 && !loaded; i++) {
        try { await page.goto('http://127.0.0.1:8765/', { waitUntil: 'load' }); loaded = true; }
        catch (error) { if (i === 29) throw error; await pause(100); }
      }
      await page.addStyleTag({ content: '.полотно { visibility: hidden !important }' });
      const img = page.locator('.фон-сайта img');
      await img.evaluate(el => el.decode());
      assert.match(decodeURIComponent(await img.evaluate(el => el.currentSrc)), /фон-анимация\.gif$/);
      await pause(600);
      const first = await page.screenshot();
      let changed = false;
      for (let i = 0; i < 4 && !changed; i++) {
        await pause(2000);
        changed = !first.equals(await page.screenshot());
      }
      assert(changed, `Фон не меняется: ${viewport.width}x${viewport.height}`);
      await page.emulateMedia({ reducedMotion: 'reduce' });
      await page.waitForFunction(() => decodeURIComponent(document.querySelector('.фон-сайта img').currentSrc).endsWith('фон-сайта-статичный.webp'));
      await img.evaluate(el => el.decode());
      assert.match(decodeURIComponent(await img.evaluate(el => el.currentSrc)), /фон-сайта-статичный\.webp$/);
      await pause(500);
      const still = await page.screenshot();
      await pause(1500);
      assert(still.equals(await page.screenshot()), 'Фон должен оставаться неподвижным при уменьшении движения');
      await page.close();
      console.log(`Фон проверен: ${viewport.width}x${viewport.height}`);
    }
  } finally {
    if (browser) await browser.close();
    server.kill();
  }
})().catch(error => {
  console.error(error);
  console.error(`::error::${String(error).replace(/[\r\n]/g, ' ')}`);
  process.exitCode = 1;
});
