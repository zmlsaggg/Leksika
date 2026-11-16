import { createClient } from '@supabase/supabase-js';

// Publishable key предназначен для браузера; доступ ограничивают RLS-политики.
const db = createClient('https://lgvezhinbqaipmwxkeug.supabase.co', 'sb_publishable_kzEeg_d0NRhb3oxcpICfLg_uDDN1INJ');
const auth = document.getElementById('авторизация');
const page = document.querySelector('[data-сообщество]');
let user = null;
let objectUrls = [];
let номерЗагрузки = 0;
const element = (tag, text) => { const el = document.createElement(tag); el.textContent = text; return el; };
function message(text) { const el = document.getElementById('статус-сообщества'); if (el) el.textContent = text; }
function showAuth() {
  if (!auth) return;
  auth.replaceChildren();
  const label = element('span', user ? 'GitHub: ' + (user.user_metadata?.user_name || 'участник') : 'Вход через GitHub');
  const button = element('button', user ? 'Выйти' : 'Войти');
  button.type = 'button';
  button.addEventListener('click', async () => {
    button.disabled = true;
    const result = user ? await db.auth.signOut() : await db.auth.signInWithOAuth({provider:'github', options: {redirectTo: location.origin + location.pathname}});
    if (result.error) { auth.append(element('span', 'Ошибка входа: ' + result.error.message)); button.disabled = false; }
  });
  auth.append(label, button);
  if (user) { const profile = element("a", "Профиль"); profile.href = "профиль.html"; auth.append(profile); }
}
// Каждая тема имеет независимую оценку и список ответов.
function topicActions(card, row, initial) {
  let mine = initial?.моя || 0;
  const actions = element('div', ''); actions.className = 'форум-действия';
  const up = element('button', '♡ Нравится'); const down = element('button', '↓ Не нравится');
  const score = element('strong', String(initial?.счёт ?? '—')); score.className = 'форум-оценка';
  score.setAttribute('aria-label', 'Оценка темы');
  const replies = element('button', 'Комментарии ↘');
  for (const button of [up, down, replies]) button.type = 'button';
  const errors = element('p', ''); errors.className = 'форум-ошибка'; errors.setAttribute('role', 'status');
  function paint() { up.setAttribute('aria-pressed', String(mine === 1)); down.setAttribute('aria-pressed', String(mine === -1)); }
  paint();
  async function vote(value) {
    if (!user) { errors.textContent = 'Чтобы оценить тему, войдите через GitHub.'; return; }
    up.disabled = down.disabled = true; errors.textContent = '';
    try {
      const result = await db.rpc('оценить_тему', {тема: row.id, значение: value});
      if (result.error) throw result.error;
      const updated = await db.rpc('оценки_тем', {темы: [row.id]});
      if (updated.error || !updated.data?.length) throw updated.error || new Error('Нет оценки');
      mine = updated.data[0].моя; score.textContent = String(updated.data[0].счёт); paint();
    } catch (_) { errors.textContent = 'Не удалось обновить оценку. Попробуйте ещё раз.'; }
    finally { up.disabled = down.disabled = false; }
  }
  up.addEventListener('click', () => vote(1)); down.addEventListener('click', () => vote(-1));
  const area = element('div', ''); area.className = 'форум-ответы'; area.hidden = true;
  const items = element('div', ''); const replyStatus = element('p', ''); replyStatus.setAttribute('role', 'status');
  const form = document.createElement('form');
  const label = element('label', 'Ваш комментарий');
  const input = document.createElement('textarea'); input.rows = 3; input.maxLength = 2000; input.required = true; input.placeholder = 'Поделитесь мыслью…'; label.append(input);
  const submit = element('button', 'Отправить комментарий ↗'); submit.type = 'submit'; form.append(label, submit);
  area.append(items, form, replyStatus);
  async function loadReplies() {
    replyStatus.textContent = 'Загружаем комментарии…';
    const result = await db.from('ответы_темы').select('автор,текст,создано').eq('тема', row.id).order('создано', {ascending:false}).limit(100);
    if (result.error) { replyStatus.textContent = 'Не удалось загрузить комментарии. Откройте их снова, чтобы повторить.'; return; }
    items.replaceChildren();
    if (!result.data.length) items.append(element('p', 'Пока нет комментариев. Начните разговор.'));
    for (const reply of [...result.data].reverse()) {
      const entry = element('div', ''); entry.className = 'форум-ответ';
      entry.append(element('small', 'Участник ' + reply.автор.slice(0, 8) + ' · ' + new Date(reply.создано).toLocaleDateString('ru-RU')), element('p', reply.текст));
      items.append(entry);
    }
    replyStatus.textContent = result.data.length === 100 ? 'Показаны последние 100 комментариев.' : '';
  }
  replies.setAttribute('aria-expanded', 'false');
  replies.addEventListener('click', () => { area.hidden = !area.hidden; replies.setAttribute('aria-expanded', String(!area.hidden)); if (!area.hidden) loadReplies().catch(() => { replyStatus.textContent = 'Не удалось связаться с сервером.'; }); });
  form.addEventListener('submit', async e => {
    e.preventDefault();
    if (!user) { replyStatus.textContent = 'Чтобы комментировать, войдите через GitHub.'; return; }
    const text = input.value.trim(); if (!text) { replyStatus.textContent = 'Напишите комментарий.'; return; }
    submit.disabled = true;
    try {
      const result = await db.from('ответы_темы').insert({тема: row.id, автор: user.id, текст: text});
      if (result.error) throw result.error;
      input.value = ''; await loadReplies();
    } catch (_) { replyStatus.textContent = 'Не удалось отправить комментарий.'; }
    finally { submit.disabled = false; }
  });
  actions.append(up, score, down, replies); card.append(actions, errors, area);
}
async function load() {
  if (!page) return;
  if (page.dataset.сообщество === 'профиль') {
    const info = document.getElementById('профиль-информация');
    if (!user) { info.textContent = 'Войдите через GitHub, чтобы увидеть профиль.'; return; }
    info.textContent = 'Загружаем профиль…';
    const [programs, topics] = await Promise.all(['программы', 'темы'].map(table => db.from(table).select('id', {count:'exact', head:true}).eq('автор', user.id)));
    info.textContent = programs.error || topics.error ? 'Не удалось загрузить профиль.' : 'GitHub: ' + (user.user_metadata?.user_name || 'участник') + ' · Программ: ' + programs.count + ' · Тем: ' + topics.count;
    return;
  }
  const kind = page.dataset.сообщество, program = kind === 'программы';
  const table = program ? 'программы' : 'темы';
  const list = document.getElementById('публикации');
  const status = document.getElementById('результат-загрузки');
  const current = ++номерЗагрузки;
  if (status) status.textContent = 'Загружаем публикации…';
  const { data, error } = await db.from(table).select('*').order('создано', {ascending:false}).limit(50);
  if (current !== номерЗагрузки) return;
  if (error) {
    if (status) {
      const box = element('div', ''); box.className = 'загрузка-списка';
      const missing = ['PGRST205', '42P01'].includes(error.code);
      box.append(element('p', missing ? 'Список пока недоступен: таблицы сообщества не найдены в базе.' : 'Не удалось загрузить список. Проверьте подключение и попробуйте ещё раз.'));
      const retry = element('button', 'Повторить'); retry.type = 'button'; retry.addEventListener('click', () => load()); box.append(retry);
      status.replaceChildren(box);
    }
    return;
  }
  let ratings = new Map();
  if (!program && data.length) {
    const result = await db.rpc('оценки_тем', {темы:data.map(row => row.id)});
    if (current !== номерЗагрузки) return;
    if (result.error) { if (status) status.textContent = 'Оценки временно недоступны. Обновите страницу.'; }
    else { ratings = new Map(result.data.map(value => [value.тема, value])); if (status) status.textContent = ''; }
  } else if (status) status.textContent = '';
  objectUrls.forEach(url => URL.revokeObjectURL(url)); objectUrls = [];
  list.replaceChildren();
  if (!data.length) list.append(element('p', 'Пока нет публикаций.'));
  for (const row of data) {
    const card = element('article', ''); card.className = program ? 'публикация карточка-игры' : 'публикация';
    if (program) {
      card.dataset.категория = 'пользовательские';
      const active = document.querySelector('[data-фильтр][aria-pressed="true"]');
      card.hidden = active?.dataset.фильтр === 'оригинальные';
      const tag = element('div', 'ОТ СООБЩЕСТВА / .LEK'); tag.className = 'карточка-игры-верх'; card.append(tag);
      const icon = element('div', '⌘'); icon.className = 'игровой-символ'; icon.setAttribute('aria-hidden', 'true'); card.append(icon);
    }
    card.append(element('h3', row.название));
    const meta = element('p', 'Автор: ' + row.автор.slice(0, 8) + ' · ' + new Date(row.создано).toLocaleDateString('ru-RU'));
    card.append(meta);
    if (program) {
      const details = document.createElement('details'); const summary = element('summary', 'Посмотреть исходник'); const pre = element('pre', row.исходник); details.append(summary, pre); card.append(details);
      const link = element('a', 'Скачать .lek ↓');
      link.href = URL.createObjectURL(new Blob([row.исходник], {type:'text/plain;charset=utf-8'})); objectUrls.push(link.href); link.download = 'program.lek'; card.append(link);
    } else { card.append(element('p', row.текст)); topicActions(card, row, ratings.get(row.id)); }
    if (user && row.автор !== user.id) {
      const button = element('button', 'Пожаловаться'); button.type = 'button';
      button.addEventListener('click', async () => {
        button.disabled = true;
        const {error} = await db.from(program ? 'жалобы_программы' : 'жалобы_темы').insert({автор:user.id, цель:row.id});
        if (error) { message(error.code === '23505' ? 'Вы уже пожаловались на эту публикацию.' : 'Не удалось отправить жалобу.'); button.disabled = false; }
        else { message('Жалоба отправлена.'); button.textContent = 'Жалоба отправлена'; }
      }); card.append(button);
    }
    list.append(card);
  }
}
if (page && page.dataset.сообщество !== 'профиль') {
  const form = document.getElementById('форма-публикации');
  form.addEventListener('submit', async e => {
    e.preventDefault();
    if (!user) { message('Для публикации войдите через GitHub.'); return; }
    const program = page.dataset.сообщество === 'программы';
    const title = document.getElementById('название-публикации').value.trim();
    let content = document.getElementById('текст-публикации').value;
    const file = document.getElementById('файл-программы');
    if (program && file && file.files.length) {
      const f = file.files[0];
      if (!/\.lek$/i.test(f.name) || f.size > 32768) { message('Нужен файл .lek размером не более 32 КБ.'); return; }
      content = await f.text();
    }
    if (!title || !content.trim() || (program && new TextEncoder().encode(content).length > 32768)) { message('Заполните название и текст; исходник — не более 32 КБ.'); return; }
    const submit = form.querySelector('button[type=submit]'); submit.disabled = true;
    try {
      const row = program ? {автор:user.id, название:title, исходник:content} : {автор:user.id, название:title, текст:content};
      const {error} = await db.from(program ? 'программы' : 'темы').insert(row);
      if (error) { message('Не удалось опубликовать: проверьте настройку базы и размер текста.'); return; }
      form.reset(); await load(); message('Опубликовано.');
    } catch (_) { message('Не удалось связаться с Supabase.'); } finally { submit.disabled = false; }
  });
}
db.auth.onAuthStateChange((_event, session) => {
  user = session?.user || null; showAuth();
  // Запросы к Supabase нельзя ожидать внутри callback блокировки сессии.
  setTimeout(() => load().catch(() => { const status = document.getElementById('результат-загрузки'); if (status) status.textContent = 'Не удалось связаться с сервером.'; }), 0);
});
