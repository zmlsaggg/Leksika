const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const textmate = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

async function main() {
  const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
  await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
  const file = path.join(__dirname, '../syntaxes/leksika.tmLanguage.json');
  const raw = JSON.parse(fs.readFileSync(file, 'utf8'));
  const registry = new textmate.Registry({
    onigLib: Promise.resolve({
      createOnigScanner: patterns => new oniguruma.OnigScanner(patterns),
      createOnigString: value => new oniguruma.OnigString(value),
    }),
    loadGrammar: () => raw,
  });
  const grammar = await registry.loadGrammar('source.leksika');
  let stack = textmate.INITIAL;
  function line(text) {
    const result = grammar.tokenizeLine(text, stack);
    stack = result.ruleStack;
    return (fragment, scope) => {
      const pos = text.indexOf(fragment);
      assert.notEqual(pos, -1, `Нет текста ${fragment}`);
      const token = result.tokens.find(t => t.startIndex <= pos && t.endIndex > pos);
      assert(token && token.scopes.includes(scope), `${text}: ${fragment} не имеет ${scope}`);
    };
  }
  let token = line('пусть сумма равно пять.');
  token('пусть', 'storage.modifier.leksika');
  token('равно', 'keyword.operator.word.leksika');
  token('пять', 'constant.numeric.word.leksika');
  token = line('заметка комментарий');
  token('комментарий', 'comment.line.leksika');
  token = line('сказать речь Привет ((мир)) (сумма плюс (два))');
  token('((', 'constant.character.escape.leksika');
  token('два', 'constant.numeric.word.leksika');
  token = line('ещё текст конец речи.');
  token('ещё', 'string.unquoted.speech.leksika');
  token('конец речи', 'punctuation.definition.string.end.leksika');
  token = line('если сумма больше нуля то');
  token('если', 'keyword.control.leksika');
  token('нуля', 'constant.numeric.word.leksika');
  token = line('сказать речь конецречи.');
  token('конецречи', 'punctuation.definition.string.end.leksika');
  console.log('TextMate: проверки пройдены');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
