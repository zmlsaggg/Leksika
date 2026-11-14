"""Инициализация модулей в переходной VM без исполнения узлов AST."""
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
loader = ROOT / 'лексика' / 'загрузка.lek'
if not getattr(sys, '_лексика_загрузка', False):
    exec(compile(loader.read_text(encoding='utf-8'), str(loader), 'exec'),
         {'__name__': 'лексика.загрузка', '__file__': str(loader)})

from лексика import грамматика, исполнитель, машина
from лексика.ошибки import ОшибкаСмысла


class МодулиVM(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def module(self, name, text):
        (self.root / (name + '.lek')).write_text(text, encoding='utf-8')

    def run_program(self, code, tree=False):
        output = []
        engine = исполнитель.Исполнитель(вывод=output.append, поиск_модулей=[str(self.root)])
        nodes = грамматика.разобрать(code)
        if tree:
            engine.исполнить(nodes)
            return output, None
        def no_ast(_):
            raise AssertionError('модуль не должен исполняться через AST')
        engine.шаг = no_ast
        vm = машина.исполнить(nodes, engine)
        return output, vm

    def test_действия_правила_и_повторное_подключение(self):
        self.module('данные', 'пусть число равно 4.\n'
                    'сказать речь начало конец речи.\n'
                    'правило удвоить принимает аргумент повторять\n'
                    'вернуть аргумент умножить на два.\nконец правила.\n')
        code = 'подключить данные. подключить данные. сказать удвоить(число).'
        reference, _ = self.run_program(code, tree=True)
        output, vm = self.run_program(code)
        self.assertEqual(reference, ['начало', '8'])
        self.assertEqual(output, reference)
        self.assertIn('данные', vm.модули)
        self.assertEqual(sum(c[0] == 'подключить' for c in vm.программа.команды), 2)

    def test_вложенные_циклические_модули(self):
        self.module('первый', 'сказать речь первый конец речи. подключить второй. '
                    'сказать речь первый конец речи.')
        self.module('второй', 'сказать речь второй конец речи. подключить первый.')
        code = 'подключить первый. сказать речь конец конец речи.'
        expected, _ = self.run_program(code, tree=True)
        output, vm = self.run_program(code)
        self.assertEqual(output, expected)
        self.assertEqual(output, ['первый', 'второй', 'первый', 'конец'])
        self.assertEqual(vm.модули, {'первый', 'второй'})

    def test_переходы_и_пустой_модуль(self):
        self.module('пустой', '')
        self.module('счётчик', 'пусть счёт равно ноль.\n'
                    'повторить три раза\nувеличить счёт на один.\nконец повторить.\n'
                    'если счёт равно три то\nсказать счёт.\nконец если.')
        code = 'подключить пустой. подключить счётчик. сказать счёт.'
        expected, _ = self.run_program(code, tree=True)
        output, _ = self.run_program(code)
        self.assertEqual(output, expected)
        self.assertEqual(output, ['3', '3'])

    def test_цикл_вызывающего_и_цикл_модуля_независимы(self):
        self.module('модуль', 'повторить два раза\n'
                    'сказать речь модуль конец речи.\nконец повторить.')
        code = ('повторить три раза\nподключить модуль.\n'
                'сказать речь основной конец речи.\nконец повторить.')
        expected, _ = self.run_program(code, tree=True)
        output, _ = self.run_program(code)
        self.assertEqual(output, expected)
        self.assertEqual(output, ['модуль', 'модуль', 'основной',
                                  'основной', 'основной'])

    def test_подключение_внутри_правила(self):
        self.module('локальный', 'пусть результат равно 42. сказать результат.')
        code = ('правило образец повторять\nподключить локальный.\n'
                'вернуть результат.\nконец правила.\nсказать образец().')
        expected, _ = self.run_program(code, tree=True)
        output, vm = self.run_program(code)
        self.assertEqual(output, expected)
        self.assertEqual(output, ['42', '42'])
        self.assertEqual(len(vm.и.области), 1)

    def test_чтение_имени_не_создаёт_ast_узел(self):
        from unittest.mock import patch
        nodes = грамматика.разобрать('пусть сумма равно 42. сказать сумма.')
        engine = исполнитель.Исполнитель(вывод=lambda _: None)
        with patch.object(грамматика, 'Имя', side_effect=AssertionError('AST при исполнении')):
            машина.исполнить(nodes, engine)

    def test_управление_чужим_циклом_не_делает_бесконечный_переход(self):
        for action in ('прервать', 'продолжить'):
            with self.subTest(action=action):
                self.module('действие', action + '.')
                code = 'повторить два раза\nподключить действие.\nконец повторить.'
                with self.assertRaisesRegex(ОшибкаСмысла, 'только внутри цикла'):
                    self.run_program(code)

    def test_разрешение_чтения_модуля(self):
        from unittest.mock import patch
        from лексика import безопасность
        self.module('данные', 'сказать 42.')
        with patch.object(безопасность, 'разрешить_файл', return_value=False):
            with self.assertRaisesRegex(ОшибкаСмысла, 'запрещено политикой'):
                self.run_program('подключить данные.')

    def test_ошибка_инициализации_очищает_кадры_и_области(self):
        self.module('поломка', 'повторить два раза\nсказать неизвестное.\nконец повторить.')
        code = ('правило образец повторять\nподключить поломка.\n'
                'конец правила.\nобразец().')
        engine = исполнитель.Исполнитель(вывод=lambda _: None, поиск_модулей=[str(self.root)])
        vm = машина.Машина(engine)
        with self.assertRaises(ОшибкаСмысла):
            vm.исполнить(грамматика.разобрать(code))
        self.assertEqual(len(engine.области), 1)
        self.assertEqual(len(vm.кадры), 1)
        self.assertEqual(vm.кадр().циклы, [])
        self.assertIs(engine.вызвать.__func__, исполнитель.Исполнитель.вызвать)

    def test_цикл_при_ошибке_не_оставляет_область(self):
        engine = исполнитель.Исполнитель(вывод=lambda _: None, поиск_модулей=[str(self.root)])
        vm = машина.Машина(engine)
        code = ('пусть предметы равно перечень(1).\n'
                'для каждого предмет из предметы повторять\n'
                'сказать неизвестное.\nконец для.')
        with self.assertRaises(ОшибкаСмысла):
            vm.исполнить(грамматика.разобрать(code))
        self.assertEqual(len(engine.области), 1)
        self.assertEqual(vm.кадр().циклы, [])
        self.assertIs(engine.вызвать.__func__, исполнитель.Исполнитель.вызвать)

    def test_дубликат_правила_отклоняется_без_инициализации(self):
        self.module('конфликт', 'сказать речь нельзя конец речи.\n'
                    'правило вещь повторять вернуть 1. конец правила.')
        code = ('правило вещь повторять вернуть 2. конец правила.\n'
                'подключить конфликт.')
        output = []
        engine = исполнитель.Исполнитель(вывод=output.append, поиск_модулей=[str(self.root)])
        with self.assertRaisesRegex(ОшибкаСмысла, 'уже объявлено'):
            машина.исполнить(грамматика.разобрать(code), engine)
        self.assertEqual(output, [])


if __name__ == '__main__':
    unittest.main()
