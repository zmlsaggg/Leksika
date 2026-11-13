(() => {
  "use strict";
  const $ = id => document.getElementById(id);
  const panel = $("ии-панель");
  const checkbox = $("ии-включить");
  const historyNode = $("ии-история");
  const form = $("ии-форма-чата");
  const input = $("ии-сообщение");
  const send = $("ии-отправить");
  const keyInput = $("ии-ключ");
  const history = [];
  const welcome = "Привет! Я Лексика AI. Можешь спрашивать меня о программировании, технологиях, науке, творчестве или вообще о чём угодно.\n\nЕсли захочешь, помогу познакомиться с языком программирования Лексика — русским языком с кириллическим синтаксисом. С чего начнём?";
  const projectInfo = window.LEKSIKA_AI_KNOWLEDGE || "Лексика — русскоязычный язык программирования; https://leksika.gleeze.com/";
  const system = "Ты — Лексика AI, дружелюбный универсальный собеседник. По умолчанию отвечай на русском. Можно обсуждать любые темы. Не навязывай тему языка в каждом сообщении. Если разговор естественно касается программирования на русском, расскажи о Лексике и предложи небольшой пример, но без рекламы и давления. Отличай факты от планов.\n\nСПРАВКА О ПРОЕКТЕ:\n" + projectInfo;
  try {
    const saved = JSON.parse(sessionStorage.getItem("leksika-ai-chat-v1") || "[]");
    if (Array.isArray(saved)) saved.filter(m => m && ["user","assistant"].includes(m.role) && typeof m.content === "string").slice(-24).forEach(m => history.push(m));
  } catch (_) {}
  if (!history.length) history.push({role:"assistant",content:welcome});
  const escapeHtml = value => String(value).replace(/[&<>"]/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
  function appendMessage(message) {
    const row = document.createElement("article");
    row.className = "ai-message ai-message-" + message.role;
    const avatar = document.createElement("div");
    avatar.className = "ai-message-avatar";
    avatar.textContent = message.role === "assistant" ? "Л" : "Вы";
    const body = document.createElement("div");
    body.className = "ai-message-body";
    const parts = String(message.content).split(/(```[a-zA-Z]*\n[\s\S]*?\n```)/g);
    parts.forEach(part => {
      if (!part) return;
      const match = part.match(/^```[a-zA-Z]*\n([\s\S]*?)\n```$/);
      if (match) {
        const pre = document.createElement("pre");
        const code = document.createElement("code");
        code.textContent = match[1];
        pre.appendChild(code);
        body.appendChild(pre);
      } else {
        const p = document.createElement("p");
        p.textContent = part;
        body.appendChild(p);
      }
    });
    row.append(avatar, body);
    historyNode.appendChild(row);
  }
  function render() {
    historyNode.replaceChildren();
    history.forEach(appendMessage);
    historyNode.scrollTop = historyNode.scrollHeight;
  }
  function persist() {
    try { sessionStorage.setItem("leksika-ai-chat-v1", JSON.stringify(history.slice(-24))); } catch (_) {}
  }
  function getText(result) {
    const value = result && result[0] && result[0].generated_text;
    if (typeof value === "string") return value;
    if (Array.isArray(value)) {
      const last = [...value].reverse().find(item => item && item.role === "assistant");
      return last && last.content || "";
    }
    return "";
  }
  async function ask(messages) {
    if ($("ии-источник").value === "huggingface") {
      const generator = window.LEKSIKA_AI_GENERATOR;
      if (!generator) throw new Error("Сначала загрузите модель Hugging Face в настройках.");
      const result = await generator(messages, {max_new_tokens:700,temperature:.65,do_sample:true,repetition_penalty:1.08});
      const answer = getText(result);
      if (!answer) throw new Error("Модель вернула пустой ответ.");
      return answer;
    }
    const key = keyInput.value.trim();
    const model = $("ии-модель").value.trim();
    const endpoint = $("ии-адрес").value.trim();
    const provider = $("ии-режим").value;
    if (!key || !model || !endpoint) throw new Error("Заполни модель, адрес и API-ключ в настройках.");
    const url = new URL(endpoint);
    const local = ["localhost","127.0.0.1","::1"].includes(url.hostname);
    if (url.protocol !== "https:" && !(url.protocol === "http:" && local)) throw new Error("Разрешён только HTTPS, кроме локального адреса.");
    if (url.username || url.password || [...url.searchParams.keys()].some(k => /key|token|secret|auth/i.test(k))) throw new Error("Не помещай секреты в URL.");
    const headers = {"Content-Type":"application/json"};
    let body;
    if (provider === "anthropic") {
      headers["x-api-key"] = key;
      headers["anthropic-version"] = "2023-06-01";
      headers["anthropic-dangerous-direct-browser-access"] = "true";
      body = JSON.stringify({model,max_tokens:700,system,messages:messages.filter(m=>m.role!=="system")});
    } else if (provider === "gemini") {
      url.pathname = url.pathname.replace("/MODEL_ID","/"+encodeURIComponent(model));
      headers["x-goog-api-key"] = key;
      body = JSON.stringify({systemInstruction:{parts:[{text:system}]},contents:messages.filter(m=>m.role!=="system").map(m=>({role:m.role==="assistant"?"model":"user",parts:[{text:m.content}]})),generationConfig:{temperature:.65,maxOutputTokens:700}});
    } else {
      headers.Authorization = "Bearer " + key;
      body = JSON.stringify({model,messages,temperature:.65,max_tokens:700});
    }
    const response = await fetch(url.href,{method:"POST",headers,body});
    const data = await response.json().catch(()=>({}));
    if (!response.ok) throw new Error(data.error && (data.error.message || data.error.type) || "HTTP " + response.status);
    let answer = "";
    if (provider === "anthropic") answer = (data.content||[]).filter(x=>x.type==="text").map(x=>x.text).join("\n");
    else if (provider === "gemini") answer = ((((data.candidates||[])[0]||{}).content||{}).parts||[]).map(x=>x.text||"").join("\n");
    else answer = data.choices && data.choices[0] && data.choices[0].message && data.choices[0].message.content;
    if (typeof answer !== "string" || !answer) throw new Error("Неизвестный формат ответа API.");
    return answer;
  }
  render();
  document.querySelectorAll("[data-ai-mode]").forEach(button => button.addEventListener("click", () => {
    const mode = button.dataset.aiMode;
    document.querySelectorAll("[data-ai-mode]").forEach(item => {
      const active = item === button;
      item.classList.toggle("is-active",active);
      item.setAttribute("aria-selected",String(active));
    });
    document.querySelectorAll("[data-ai-panel]").forEach(item => {
      const active = item.dataset.aiPanel === mode;
      item.hidden = !active;
      item.classList.toggle("is-active",active);
    });
    if (mode === "chat") render();
  }));
  form.addEventListener("submit", async event => {
    event.preventDefault();
    if (!checkbox.checked || send.disabled) return;
    const text = input.value.trim();
    if (!text) return;
    input.value = "";
    history.push({role:"user",content:text});
    persist();
    render();
    send.disabled = true;
    const pending = {role:"assistant",content:"Думаю…"};
    appendMessage(pending);
    try {
      const answer = await ask([{role:"system",content:system},...history.slice(-16)]);
      history.push({role:"assistant",content:answer});
      if (history.length > 24) history.splice(1,history.length-24);
    } catch (error) {
      history.push({role:"assistant",content:"Не удалось получить ответ: "+error.message+(error instanceof TypeError?"\nВозможно, API блокирует запросы через CORS.":"")});
    } finally {
      persist();
      render();
      send.disabled = false;
      input.focus();
    }
  });
  input.addEventListener("keydown",event=>{
    if(event.key==="Enter"&&!event.shiftKey){event.preventDefault();form.requestSubmit();}
  });
  $("ии-очистить-чат").addEventListener("click",()=>{
    history.splice(0,history.length,{role:"assistant",content:welcome});
    persist();
    render();
  });
  const applyButton = $("ии-применить");
  const output = $("ии-вывод");
  let suggested = "";
  const observer = new MutationObserver(()=>{
    const match = output.textContent.match(/```(?:lek|leksika|text)?\s*\n([\s\S]*?)\n```/i);
    suggested = match ? match[1].trim() : "";
    applyButton.hidden = !suggested;
  });
  observer.observe(output,{childList:true,subtree:true,characterData:true});
  applyButton.addEventListener("click",()=>{
    if(!suggested)return;
    $("код").value=suggested;
    $("код").dispatchEvent(new Event("input",{bubbles:true}));
    $("состояние").textContent="Предложенное исправление применено — проверь и запусти код.";
    applyButton.hidden=true;
    suggested="";
  });
  const checkboxPanel = $("ии-панель");
  checkbox.addEventListener("change",()=>{checkboxPanel.hidden=!checkbox.checked;});
})();