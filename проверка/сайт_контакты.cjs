// Проверка «человеческого следа» сайта: контакты, автор, Organization schema,
// статьи в карте сайта и отсутствие битых внутренних ссылок на новых страницах.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const КОРЕНЬ = path.join(__dirname, '..');
const САЙТ = 'https://leksika.gleeze.com';

const прочитать = файл => fs.readFileSync(path.join(КОРЕНЬ, файл), 'utf8');

// Все HTML-страницы сайта, включая новые разделы.
const страницы = [
  'index.html',
  'playground.html',
  'игры.html',
  'форум.html',
  'профиль.html',
  'статьи.html',
  'о-проекте.html',
  'контакты.html',
  'статьи/падежи-вместо-операторов.html',
  'статьи/перенос-движка-с-python-на-c11.html',
  'статьи/песочница-что-умеет-и-чего-не-умеет.html',
  'статьи/почему-проект-пишет-о-своих-недостатках.html',
  'статьи/три-задачи-на-лексике.html',
  'статьи/русские-языки-программирования-до-лексики.html'
];

for (const страница of страницы) {
  assert(fs.existsSync(path.join(КОРЕНЬ, страница)), `Нет страницы: ${страница}`);
}

// 1. Organization schema и контактные данные должны быть на каждой странице.
for (const страница of страницы) {
  const html = прочитать(страница);
  assert.match(html, /"@type"\s*:\s*"Organization"/, `${страница}: нет Organization в JSON-LD`);
  assert.match(html, /leksika\.gleeze\.com\/#организация/, `${страница}: нет @id организации`);
  assert.match(html, /github\.com\/zmlsaggg\/Leksika\/issues/, `${страница}: нет ссылки на задачи как канала связи`);
}

// 2. Структурированные данные должны быть разбираемым JSON.
for (const страница of страницы) {
  const html = прочитать(страница);
  const блоки = [...html.matchAll(/<script type="application\/ld\+json">([\s\S]*?)<\/script>/g)];
  assert(блоки.length > 0, `${страница}: нет JSON-LD`);
  for (const [, тело] of блоки) {
    const данные = JSON.parse(тело);
    assert.equal(данные['@context'], 'https://schema.org', `${страница}: неверный @context`);
  }
}

// 3. У автора должен быть публичный профиль, а не только название проекта.
assert.match(прочитать('о-проекте.html'), /"@type"\s*:\s*"Person"[\s\S]*?github\.com\/zmlsaggg/, 'о-проекте.html: нет Person с профилем автора');
assert.match(прочитать('контакты.html'), /"@type"\s*:\s*"ContactPage"/, 'контакты.html: нет ContactPage');
assert.match(прочитать('контакты.html'), /"@type"\s*:\s*"ContactPoint"/, 'контакты.html: нет ContactPoint');

// 4. Ссылки на контакты и проект должны быть доступны с главной.
const главная = прочитать('index.html');
assert.match(главная, /href="контакты\.html"/, 'index.html: нет ссылки на контакты');
assert.match(главная, /href="о-проекте\.html"/, 'index.html: нет ссылки на страницу о проекте');
assert.match(главная, /href="статьи\.html"/, 'index.html: нет ссылки на статьи');
assert.match(главная, /"@type"\s*:\s*"FAQPage"/, 'index.html: нет FAQPage');

// 5. Содержательные текстовые страницы должны быть непустыми.
for (const страница of ['о-проекте.html', 'контакты.html', 'статьи.html', ...страницы.filter(с => с.startsWith('статьи/'))]) {
  const текст = прочитать(страница)
    .replace(/<script[\s\S]*?<\/script>/g, ' ')
    .replace(/<[^>]+>/g, ' ')
    .replace(/\s+/g, ' ');
  const слов = текст.split(' ').filter(с => с.length > 2).length;
  assert(слов >= 250, `${страница}: слишком мало текста (${слов} слов)`);
  assert.match(прочитать(страница), /<h1[\s>]/, `${страница}: нет h1`);
}

// 6. Карта сайта должна перечислять все новые страницы.
const карта = прочитать('sitemap.xml');
const адреса = { 'index.html': `${САЙТ}/` };
for (const страница of страницы.filter(с => с !== 'профиль.html')) {
  const адрес = адреса[страница] || `${САЙТ}/${страница}`;
  assert(карта.includes(`<loc>${адрес}</loc>`), `sitemap.xml: нет ${страница}`);
}

// 7. Внутренние ссылки на страницах должны вести на существующие файлы.
const проверено = new Set();
for (const страница of страницы) {
  const html = прочитать(страница);
  const папка = path.posix.dirname(страница);
  for (const [, ссылка] of html.matchAll(/href="([^"]+)"/g)) {
    if (/^(https?:|#|mailto:|data:)/.test(ссылка)) continue;
    const [путь] = ссылка.split('#');
    if (!путь) continue;
    const файл = путь.split('?')[0];
    const цель = path.posix.normalize(path.posix.join(папка === '.' ? '' : папка, файл));
    if (проверено.has(цель)) continue;
    проверено.add(цель);
    const абсолютный = path.join(КОРЕНЬ, цель);
    assert(fs.existsSync(абсолютный), `${страница}: битая ссылка ${ссылка}`);
  }
}

// 8. llms.txt должен называть автора, контакты и статьи.
const llms = прочитать('llms.txt');
assert.match(llms, /zmlsaggg/, 'llms.txt: не указан автор');
assert.match(llms, /контакты\.html/, 'llms.txt: нет страницы контактов');
assert.match(llms, /статьи\.html/, 'llms.txt: нет страницы статей');

// 9. Каждая статья должна иметь дату публикации и автора в метаданных.
for (const страница of страницы.filter(с => с.startsWith('статьи/'))) {
  const html = прочитать(страница);
  assert.match(html, /"datePublished"\s*:\s*"\d{4}-\d{2}-\d{2}"/, `${страница}: нет даты публикации`);
  assert.match(html, /"dateModified"\s*:\s*"\d{4}-\d{2}-\d{2}"/, `${страница}: нет даты изменения`);
  assert.match(html, /"@type"\s*:\s*"Article"/, `${страница}: нет Article в JSON-LD`);
  assert.match(html, /rel="noopener noreferrer"/, `${страница}: внешние ссылки без rel=noopener`);
}

// 10. Примеры кода в статьях должны работать: блок без пометки «настольный»
// исполняется учебной машиной, иначе читатель получит нерабочий код.
require('../учебная-машина.js');
let примеров = 0;
for (const страница of страницы.filter(с => с.startsWith('статьи/'))) {
  const html = прочитать(страница);
  const блоки = [...html.matchAll(/<pre><code[^>]*>([\s\S]*?)<\/code><\/pre>/g)];
  for (const [номер, блок] of блоки.entries()) {
    const подпись = (блок[1].match(/<span class="подпись-кода">([\s\S]*?)<\/span>/) || ['', ''])[1];
    if (/НАСТОЛЬНЫЙ/.test(подпись)) continue;
    const код = блок[1]
      .replace(/<span class="подпись-кода">[\s\S]*?<\/span>/, '')
      .replace(/<[^>]+>/g, '')
      .replace(/&lt;/g, '<')
      .replace(/&gt;/g, '>')
      .replace(/&amp;/g, '&');
    if (!/пусть|сказать|правило/.test(код)) continue;
    примеров++;
    try {
      global.ЛексикаУчебная.создать(код, {}, () => {});
    } catch (ошибка) {
      throw new Error(`${страница}, пример ${номер + 1}: ${ошибка.message}`);
    }
  }
}

console.log(`Проверено страниц: ${страницы.length}, ссылок: ${проверено.size}, исполняемых примеров: ${примеров}. Контакты, автор и статьи на месте.`);
