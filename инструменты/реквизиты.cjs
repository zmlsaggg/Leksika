#!/usr/bin/env node
'use strict';
// Проверка формата и контрольной суммы не подтверждает существование счёта.
function инн(value) {
  if (!/^\d{10}$|^\d{12}$/.test(value)) return false;
  const digits = Array.from(value, Number);
  const control = weights => weights.reduce((sum, weight, i) => sum + weight * digits[i], 0) % 11 % 10;
  if (digits.length === 10) return control([2,4,10,3,5,9,4,6,8]) === digits[9];
  return control([7,2,4,10,3,5,9,4,6,8]) === digits[10] && control([3,7,2,4,10,3,5,9,4,6,8]) === digits[11];
}
// Только официальная длина конкретной страны плюс общий ISO 13616 mod-97.
const IBAN_LENGTHS = Object.freeze({DE:22, GB:22, FR:27, ES:24, IT:27, NL:18, SE:24, NO:15, FI:18, CH:21, AT:20, BE:16, PL:28});
function iban(value) {
  if (!/^[A-Za-z0-9 ]+$/.test(value)) return false;
  const compact = value.replace(/ /g, '').toUpperCase();
  if (!/^[A-Z]{2}[0-9]{2}[A-Z0-9]+$/.test(compact) || IBAN_LENGTHS[compact.slice(0,2)] !== compact.length) return false;
  let remainder = 0;
  for (const char of compact.slice(4) + compact.slice(0,4)) {
    const digits = char >= 'A' ? String(char.charCodeAt(0) - 55) : char;
    for (const digit of digits) remainder = (remainder * 10 + Number(digit)) % 97;
  }
  return remainder === 1;
}
function main(args) {
  if (args.length === 1 && ['справка','--help'].includes(args[0])) {
    console.log('Использование: node инструменты/реквизиты.cjs проверить инн|iban ЗНАЧЕНИЕ');
    console.log('Локальная проверка формата и контрольной суммы, не подтверждение существования.'); return 0;
  }
  if (args.length !== 3 || args[0] !== 'проверить' || !['инн','iban'].includes(args[1])) {
    console.error('Укажите: проверить инн|iban ЗНАЧЕНИЕ'); return 2;
  }
  const valid = args[1] === 'инн' ? инн(args[2]) : iban(args[2]);
  console.log(valid ? 'Формат и контрольная сумма корректны; существование не проверено.' : 'Формат или контрольная сумма неверны либо страна IBAN не поддерживается.');
  return valid ? 0 : 1;
}
if (require.main === module) process.exitCode = main(process.argv.slice(2));
module.exports = {инн, iban};
