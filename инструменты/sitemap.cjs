// Автогенерация sitemap.xml: сканирует HTML-страницы сайта, берёт дату
// последнего коммита из Git (git log) и перезаписывает sitemap.xml.
// Запуск: npm run sitemap  (или node инструменты/sitemap.cjs)
//
// Правила:
// - включаются все *.html в корне и в статьи/, кроме noindex-страниц
//   (профиль.html) — они не должны попадать в карту;
// - плюс три документа из документы/ (РУКОВОДСТВО, СЛОВАРЬ, АРХИТЕКТУРА),
//   которые исторически входят в карту;
// - lastmod = дата последнего коммита файла; если файла ещё нет в Git
//   или git недоступен — дата изменения файла, затем сегодняшняя;
// - priority/changefreq заданы таблицей ниже; для новых страниц —
//   monthly / 0.7.
const { execFileSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const КОРЕНЬ = path.join(__dirname, '..');
const САЙТ = 'https://leksika.gleeze.com';

// Порядок и приоритеты — как в ручной карте, чтобы генерация не
// перетасовывала файл при каждом запуске.
const ПОРЯДОК = [
  { файл: 'index.html', адрес: `${САЙТ}/`, priority: '1.0', changefreq: 'weekly' },
  { файл: 'playground.html', priority: '0.9', changefreq: 'weekly' },
  { файл: 'статьи.html', priority: '0.9', changefreq: 'weekly' },
  { файл: 'статьи/падежи-вместо-операторов.html', priority: '0.8', changefreq: 'monthly' },
  { файл: 'статьи/перенос-движка-с-python-на-c11.html', priority: '0.8', changefreq: 'monthly' },
  { файл: 'статьи/песочница-что-умеет-и-чего-не-умеет.html', priority: '0.8', changefreq: 'monthly' },
  { файл: 'статьи/почему-проект-пишет-о-своих-недостатках.html', priority: '0.7', changefreq: 'monthly' },
  { файл: 'статьи/три-задачи-на-лексике.html', priority: '0.8', changefreq: 'monthly' },
  { файл: 'статьи/русские-языки-программирования-до-лексики.html', priority: '0.7', changefreq: 'monthly' },
  { файл: 'о-проекте.html', priority: '0.8', changefreq: 'monthly' },
  { файл: 'контакты.html', priority: '0.6', changefreq: 'yearly' },
  { файл: 'игры.html', priority: '0.7', changefreq: 'weekly' },
  { файл: 'форум.html', priority: '0.6', changefreq: 'daily' },
  { файл: 'документы/РУКОВОДСТВО.md', priority: '0.7', changefreq: 'monthly' },
  { файл: 'документы/СЛОВАРЬ.md', priority: '0.7', changefreq: 'monthly' },
  { файл: 'документы/АРХИТЕКТУРА.md', priority: '0.6', changefreq: 'monthly' },
];

// Страницы, которые нельзя индексировать (noindex) — в карту не попадают.
const ИСКЛЮЧИТЬ = new Set(['профиль.html']);

function датаИзGit(файл) {
  try {
    const дата = execFileSync('git', ['log', '-1', '--format=%cs', '--', файл], {
      cwd: КОРЕНЬ,
      encoding: 'utf8',
    }).trim();
    return /^\d{4}-\d{2}-\d{2}$/.test(дата) ? дата : null;
  } catch {
    return null;
  }
}

function датаИзФайла(файл) {
  try {
    const стат = fs.statSync(path.join(КОРЕНЬ, файл));
    return стат.mtime.toISOString().slice(0, 10);
  } catch {
    return new Date().toISOString().slice(0, 10);
  }
}

// Найти все HTML в корне и в статьи/, чтобы новые страницы попадали
// в карту без правки таблицы выше.
function найтиHtml() {
  const найдены = new Set();
  for (const запись of fs.readdirSync(КОРЕНЬ)) {
    if (запись.endsWith('.html')) найдены.add(запись);
  }
  const папкаСтатей = path.join(КОРЕНЬ, 'статьи');
  if (fs.existsSync(папкаСтатей)) {
    for (const запись of fs.readdirSync(папкаСтатей)) {
      if (запись.endsWith('.html')) найдены.add(`статьи/${запись}`);
    }
  }
  return [...найдены].filter(ф => !ИСКЛЮЧИТЬ.has(ф));
}

function собратьЗаписи() {
  const известные = new Map(ПОРЯДОК.map(з => [з.файл, з]));
  const записи = [];
  // Сначала известные в заданном порядке (если файл существует).
  for (const запись of ПОРЯДОК) {
    if (fs.existsSync(path.join(КОРЕНЬ, запись.файл))) записи.push(запись);
  }
  // Затем новые HTML, которых нет в таблице.
  for (const файл of найтиHtml().sort()) {
    if (!известные.has(файл)) {
      записи.push({ файл, priority: '0.7', changefreq: 'monthly' });
    }
  }
  return записи;
}

function экранировать(строка) {
  return строка.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function main() {
  const записи = собратьЗаписи();
  const строки = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">',
  ];
  for (const запись of записи) {
    const адрес = запись.адрес || `${САЙТ}/${запись.файл}`;
    const lastmod = датаИзGit(запись.файл) || датаИзФайла(запись.файл);
    строки.push(
      '  <url>',
      `    <loc>${экранировать(адрес)}</loc>`,
      `    <lastmod>${lastmod}</lastmod>`,
      `    <changefreq>${запись.changefreq}</changefreq>`,
      `    <priority>${запись.priority}</priority>`,
      '  </url>',
    );
  }
  строки.push('</urlset>', '');
  fs.writeFileSync(path.join(КОРЕНЬ, 'sitemap.xml'), строки.join('\n'), 'utf8');
  console.log(`sitemap.xml: ${записи.length} адресов.`);
}

main();
