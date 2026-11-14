const { chromium } = require('playwright-core');
const { spawn } = require('node:child_process');
const { setTimeout: pause } = require('node:timers/promises');
const assert = require('node:assert/strict');
(async () => {
  const server = spawn('python3', ['-m', 'http.server', '8767', '--bind', '127.0.0.1'], {stdio:'ignore'});
  let browser;
  try {
    browser = await chromium.launch({executablePath: process.env.CHROME_PATH || '/usr/bin/google-chrome', args:['--no-sandbox']});
    const page = await browser.newPage();
    const me = '11111111-1111-4111-8111-111111111111';
    const author = '22222222-2222-4222-8222-222222222222';
    const jwt = `e30.${Buffer.from(JSON.stringify({sub:me,exp:Math.floor(Date.now()/1000)+3600})).toString('base64url')}.signature`;
    await page.addInitScript(({me,jwt}) => localStorage.setItem('sb-lgvezhinbqaipmwxkeug-auth-token', JSON.stringify({access_token:jwt, refresh_token:'test', token_type:'bearer', expires_at: Math.floor(Date.now()/1000)+3600, expires_in:3600, user:{id:me,user_metadata:{user_name:'tester'},app_metadata:{provider:'github'},aud:'authenticated'}})), {me,jwt});
    const writes = [];
    await page.route('**/*', async route => {
      const url = route.request().url();
      if (url.startsWith('http://127.0.0.1:8767/')) return route.continue();
      if (!url.startsWith('https://lgvezhinbqaipmwxkeug.supabase.co/')) return route.abort();
      const request = route.request();
      if (request.method() === 'POST') { writes.push({url, body:request.postDataJSON()}); return route.fulfill({status:201,body:''}); }
      if (url.includes('/rest/v1/')) return route.fulfill({status:200,contentType:'application/json',headers:{'content-range':'0-0/1'},body:JSON.stringify([{id:'33333333-3333-4333-8333-333333333333',автор:author,название:'<img src=x onerror=alert(1)>',исходник:'сказать(42);',текст:'Привет!',создано:'2026-01-01T00:00:00Z',скрыто:false}])});
      return route.fulfill({status:200,contentType:'application/json',body:JSON.stringify({id:me,user_metadata:{user_name:'tester'},app_metadata:{provider:'github'},aud:'authenticated'})});
    });
    for (let i=0;;i++) { try { await page.goto('http://127.0.0.1:8767/игры.html', {waitUntil:'load'}); break; } catch(e) { if(i===30) throw e; await pause(100); } }
    await page.waitForSelector('.публикация');
    assert.equal(await page.locator('.публикация img').count(), 0);
    assert.match(await page.locator('.публикация h3').innerText(), /<img/);
    assert.match(await page.locator('#авторизация').innerText(), /tester/);
    await page.locator('.публикация button').click();
    assert(writes.some(w => decodeURI(w.url).includes('жалобы_программы') && w.body.автор === me));
    await page.locator('#название-публикации').fill('Моя программа');
    await page.locator('#текст-публикации').fill('сказать(42);');
    await page.locator('#форма-публикации button').click();
    await page.waitForFunction(() => document.getElementById('статус-сообщества').textContent.includes('Опубликовано'));
    assert(writes.some(w => decodeURI(w.url).includes('программы') && w.body.исходник === 'сказать(42);'));
    await page.goto('http://127.0.0.1:8767/форум.html');
    await page.waitForSelector('.публикация');
    await page.locator('#название-публикации').fill('Тема');
    await page.locator('#текст-публикации').fill('Текст');
    await page.locator('#форма-публикации button').click();
    await page.waitForFunction(() => document.getElementById('статус-сообщества').textContent.includes('Опубликовано'));
    assert(writes.some(w => decodeURI(w.url).includes('темы') && w.body.текст === 'Текст'));
    console.log('Интерфейс сообщества проверен с подменой Supabase API.');
  } finally { if(browser) await browser.close(); server.kill(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
