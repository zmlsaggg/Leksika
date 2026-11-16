#!/usr/bin/env python3
"""Синхронизировать статическую TextMate-грамматику с исходным словарём."""
import json
import re
import runpy
from pathlib import Path

root = Path(__file__).resolve().parents[2]
dictionary = runpy.run_path(str(root / 'лексика/словарь.lek'))
morph = runpy.run_path(str(root / 'лексика/морфология.lek'))
forms = dictionary['СЛУЖЕБНЫЕ']

def words(keys):
    return {form for key in keys for form in forms.get(key, ())}

def regexp(items):
    # \b в Oniguruma и JavaScript зависит от настроек слова: кириллица — явно.
    choices = '|'.join(re.escape(word) for word in sorted(items, key=lambda s: (-len(s), s)))
    return r'(?i)(?<![А-Яа-яЁё0-9_-])(?:' + choices + r')(?![А-Яа-яЁё0-9_-])'

control = words(['если','то','иначе','конец','пока','повторять','раз','для','каждый','вернуть','прервать','продолжить','подключить','позвать']) | {'повторить','где','почему','как','наступит'}
declaration = words(['пусть','правило','принимает'])
constants = words(['истина','ложь','ничто'])
operators = words(['равно','и','или','не','плюс','минус','больше','меньше','содержит','остаток']) | {'это','степени'}
types = {'целое','число','дробь','дробное','вещественное','логическое','строка','перечень','список','словарь','пусто'} - constants
builtins = set(dictionary['ФУНКЦИИ']) | set(dictionary['КОМАНДЫ']) | words(['сказать','спросить','положить','увеличить','уменьшить','прибавить','отнять','умножить','разделить','длина','взять','добавить','убрать','записать','прочитать','ключи']) | {'что'}
number_words = set(morph['ЧИСЛИТЕЛЬНЫЕ']) | set(morph['МНОЖИТЕЛИ']) | {'запятая','запятой','запятую'}
comments = words(['заметка'])
remaining = set().union(*forms.values()) - control - declaration - constants - operators - types - builtins - comments
patterns = [
    {'include':'#comment'}, {'include':'#speech'},
    {'name':'entity.name.function.leksika', 'match': r'(?i)(?<=правило\s)([А-Яа-яЁё][А-Яа-яЁё0-9-]*)'},
    {'name':'storage.type.leksika','match':regexp(types)},
    {'name':'storage.modifier.leksika','match':regexp(declaration)},
    {'name':'constant.language.leksika','match':regexp(constants)},
    {'name':'keyword.control.leksika','match':regexp(control)},
    {'name':'keyword.operator.word.leksika','match':regexp(operators)},
    {'name':'support.function.builtin.leksika','match':regexp(builtins)},
    {'name':'constant.numeric.word.leksika','match':regexp(number_words)},
    {'name':'keyword.other.leksika','match':regexp(remaining | {'в','из','на','от','к','по','с','до','номер','ключу'})},
    {'name':'constant.numeric.leksika','match':r'(?<![А-Яа-яЁё0-9_-])\d+(?:[.,]\d+)?(?![А-Яа-яЁё0-9_-])'},
    {'name':'keyword.operator.symbol.leksika','match':r'\+=|-=|\*=|/=|\+\+|--|==|!=|>=|<=|&&|\|\||\*\*|[=+*/%<>-]'},
    {'name':'punctuation.separator.leksika','match':r'[.,;:!?]'},
]
grammar = {'name':'Лексика','scopeName':'source.leksika','patterns':patterns,'repository':{
 'comment': {'name':'comment.line.leksika','begin':regexp(comments),'end':'$'},
 'speech': {'name':'string.unquoted.speech.leksika','begin':regexp({'речь'}),'beginCaptures':{'0':{'name':'punctuation.definition.string.begin.leksika'}},'end':r'(?i)(?<![А-Яа-яЁё0-9_-])конец[ \t]*речи(?![А-Яа-яЁё0-9_-])','endCaptures':{'0':{'name':'punctuation.definition.string.end.leksika'}},'patterns':[
   {'name':'constant.character.escape.leksika','match':r'\(\(|\)\)'},
   {'include':'#interpolation'}
 ]},
 'interpolation': {'name':'meta.interpolation.leksika','begin':r'\(','end':r'\)','patterns':[{'include':'#interpolation'},{'include':'$self'}]}
}}
path = root / 'vscode-leksika/syntaxes/leksika.tmLanguage.json'
path.write_text(json.dumps(grammar,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
