(() => {
  "use strict";

  const редактор = document.getElementById("код");
  const панель = document.getElementById("редактор-подсказки");
  if (!редактор || !панель || typeof DecompressionStream !== "function") return;

  let загрузка = null;
  let индекс = null;
  let подсказки = [];
  let выбранная = 0;
  let началоСлова = 0;
  let конецСлова = 0;
  const декодер = new TextDecoder("utf-8", { fatal: true });
  const кодировщик = new TextEncoder();
  let таблицаCRC = null;

  function crc32(байты) {
    if (!таблицаCRC) {
      таблицаCRC = new Uint32Array(256);
      for (let n = 0; n < 256; n += 1) {
        let c = n;
        for (let k = 0; k < 8; k += 1) {
          c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
        }
        таблицаCRC[n] = c >>> 0;
      }
    }
    let crc = 0xFFFFFFFF;
    for (let i = 0; i < байты.length; i += 1) {
      crc = таблицаCRC[(crc ^ байты[i]) & 0xFF] ^ (crc >>> 8);
    }
    return (crc ^ 0xFFFFFFFF) >>> 0;
  }

  async function загрузитьИндекс() {
    if (индекс) return индекс;
    if (!загрузка) {
      загрузка = (async () => {
        const адрес = new URL("ядро/русские_формы.lxd.gz", document.baseURI);
        const ответ = await fetch(адрес.href, { cache: "force-cache" });
        if (!ответ.ok) throw new Error("Бинарный словарь ещё не опубликован.");
        const сжатое = await ответ.arrayBuffer();
        const поток = new Blob([сжатое]).stream().pipeThrough(new DecompressionStream("gzip"));
        const буфер = await new Response(поток).arrayBuffer();
        const байты = new Uint8Array(буфер);
        if (байты.length < 36 ||
            String.fromCharCode(...байты.subarray(0, 8)) !== "LEKSLXD1") {
          throw new Error("Неизвестный формат словаря.");
        }
        const вид = new DataView(буфер);
        const версия = вид.getUint32(8, true);
        const размерЗаголовка = вид.getUint32(12, true);
        const количествоBig = вид.getBigUint64(16, true);
        const размерДанныхBig = вид.getBigUint64(24, true);
        const контрольнаяСумма = вид.getUint32(32, true);
        if (версия !== 1 || размерЗаголовка !== 36 ||
            количествоBig <= 0n || количествоBig > 5000000n ||
            размерДанныхBig > 134217728n) {
          throw new Error("Заголовок словаря повреждён или не поддерживается.");
        }
        const количество = Number(количествоBig);
        const размерДанных = Number(размерДанныхBig);
        const данныеНачало = 36 + (количество + 1) * 4;
        if (данныеНачало + размерДанных !== байты.length ||
            crc32(байты.subarray(36)) !== контрольнаяСумма) {
          throw new Error("Размер или контрольная сумма словаря не совпадает.");
        }
        индекс = { байты, вид, количество, данныеНачало };
        return индекс;
      })().catch((ошибка) => {
        загрузка = null;
        throw ошибка;
      });
    }
    return загрузка;
  }

  function байтыСлова(номер) {
    const таблица = 36;
    const a = индекс.вид.getUint32(таблица + номер * 4, true);
    const b = индекс.вид.getUint32(таблица + (номер + 1) * 4, true);
    return индекс.байты.subarray(индекс.данныеНачало + a, индекс.данныеНачало + b - 1);
  }

  function сравнитьБайты(a, b) {
    const длина = Math.min(a.length, b.length);
    for (let i = 0; i < длина; i += 1) {
      if (a[i] !== b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return a.length === b.length ? 0 : (a.length < b.length ? -1 : 1);
  }

  function найтиПодсказки(префикс, предел = 8) {
    const искомое = кодировщик.encode(префикс.toLowerCase());
    let низ = 0;
    let верх = индекс.количество;
    while (низ < верх) {
      const середина = низ + Math.floor((верх - низ) / 2);
      if (сравнитьБайты(байтыСлова(середина), искомое) < 0) низ = середина + 1;
      else верх = середина;
    }
    const найдено = [];
    while (низ < индекс.количество && найдено.length < предел) {
      const слово = байтыСлова(низ++);
      if (сравнитьБайты(слово.subarray(0, искомое.length), искомое) !== 0) break;
      найдено.push(декодер.decode(слово));
    }
    return найдено;
  }

  function скрыть() {
    подсказки = [];
    выбранная = 0;
    панель.replaceChildren();
    панель.hidden = true;
    редактор.removeAttribute("aria-activedescendant");
  }

  function выбрать(номер) {
    const слово = подсказки[номер];
    if (!слово) return;
    редактор.setRangeText(слово, началоСлова, конецСлова, "end");
    скрыть();
    редактор.focus();
  }

  function отрисовать(слова, начало, конец) {
    подсказки = слова;
    выбранная = 0;
    началоСлова = начало;
    конецСлова = конец;
    панель.replaceChildren();
    if (!слова.length) {
      панель.hidden = true;
      return;
    }
    слова.forEach((слово, номер) => {
      const кнопка = document.createElement("button");
      кнопка.type = "button";
      кнопка.id = "лексическое-предложение-" + номер;
      кнопка.textContent = слово;
      кнопка.setAttribute("role", "option");
      кнопка.setAttribute("aria-selected", номер === 0 ? "true" : "false");
      кнопка.addEventListener("mousedown", (событие) => событие.preventDefault());
      кнопка.addEventListener("click", () => выбрать(номер));
      панель.appendChild(кнопка);
    });
    панель.hidden = false;
    редактор.setAttribute("aria-activedescendant", "лексическое-предложение-0");
  }

  async function обновить() {
    const позиция = редактор.selectionStart;
    const передКареткой = редактор.value.slice(0, позиция);
    const совпадение = передКареткой.match(/[А-Яа-яЁё]+$/u);
    if (!совпадение || совпадение[0].length < 2 ||
        редактор.selectionStart !== редактор.selectionEnd) {
      скрыть();
      return;
    }
    const префикс = совпадение[0].toLowerCase();
    const начало = позиция - совпадение[0].length;
    try {
      await загрузитьИндекс();
      if (редактор.selectionStart !== позиция ||
          редактор.value.slice(начало, позиция).toLowerCase() !== префикс) return;
      отрисовать(найтиПодсказки(префикс), начало, позиция);
    } catch (ошибка) {
      скрыть();
      console.warn("Подсказки словоформ недоступны:", ошибка);
    }
  }

  function переместить(направление) {
    if (!подсказки.length) return;
    выбранная = (выбранная + направление + подсказки.length) % подсказки.length;
    for (let i = 0; i < подсказки.length; i += 1) {
      const кнопка = панель.children[i];
      const активна = i === выбранная;
      кнопка.setAttribute("aria-selected", активна ? "true" : "false");
      if (активна) редактор.setAttribute("aria-activedescendant", кнопка.id);
    }
  }

  редактор.addEventListener("input", обновить);
  редактор.addEventListener("click", обновить);
  редактор.addEventListener("keyup", (событие) => {
    if (!["ArrowDown", "ArrowUp", "Enter", "Escape", "Tab"].includes(событие.key)) обновить();
  });
  редактор.addEventListener("keydown", (событие) => {
    if (!подсказки.length) return;
    if (событие.key === "ArrowDown") {
      событие.preventDefault();
      переместить(1);
    } else if (событие.key === "ArrowUp") {
      событие.preventDefault();
      переместить(-1);
    } else if (событие.key === "Enter" || событие.key === "Tab") {
      событие.preventDefault();
      выбрать(выбранная);
    } else if (событие.key === "Escape") {
      скрыть();
    }
  });
  редактор.addEventListener("blur", () => {
    window.setTimeout(() => {
      if (!панель.contains(document.activeElement)) скрыть();
    }, 0);
  });
})();
