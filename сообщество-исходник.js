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
  if (status) status.textContent = '';
  objectUrls.forEach(url => URL.revokeObjectURL(url)); objectUrls = [];
  list.replaceChildren();
  if (!data.length) list.append(element('p', 'Пока нет публикаций.'));
  for (const row of data) {
    const card = element('article', ''); card.className = 'публикация';
    card.append(element('h3', row.название));
    const meta = element('p', 'Автор: ' + row.автор.slice(0, 8) + ' · ' + new Date(row.создано).toLocaleDateString('ru-RU'));
    card.append(meta);
    if (program) {
      const pre = element('pre', row.исходник); card.append(pre);
      const link = element('a', 'Скачать .lek');
      link.href = URL.createObjectURL(new Blob([row.исходник], {type:'text/plain;charset=utf-8'})); objectUrls.push(link.href); link.download = 'program.lek'; card.append(link);
    } else card.append(element('p', row.текст));
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
