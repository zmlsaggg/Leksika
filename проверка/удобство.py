"""Регрессионные проверки CLI и более свободного синтаксиса."""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def cli(*args, input=None):
    return subprocess.run([sys.executable, str(ROOT / 'лексус.lek'), *args],
                          input=input, text=True, capture_output=True, cwd=ROOT)


class Удобство(unittest.TestCase):
    def test_выражение_и_синтаксис(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'test.lek'
            p.write_text('пусть число это 2\nсказать (число плюс 3) умножить на 4', encoding='utf-8')
            for режим in ([], ['--дерево']):
                result = cli('запустить', str(p), *режим)
                self.assertEqual((result.returncode, result.stdout.strip()), (0, '20'), result.stderr)

    def test_разговорные_формы(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'разговор.lek'
            for форма in ('что', 'скажи', 'выведи', 'говори', 'напечатать', 'печатай', 'покажи'):
                p.write_text(f'пускай счёт это 42. {форма} счёт', encoding='utf-8')
                for режим in ([], ['--дерево']):
                    r = cli('запустить', str(p), *режим)
                    self.assertEqual((r.returncode, r.stdout.strip()), (0, '42'), r.stderr)
            p.write_text('пусть счёт это 2 тогда скажи счёт', encoding='utf-8')
            for режим in ([], ['--дерево']):
                r = cli('запустить', str(p), *режим)
                self.assertEqual((r.returncode, r.stdout.strip()), (0, '2'), r.stderr)
            p.write_text('что неизвестное', encoding='utf-8')
            for режим in ([], ['--дерево']):
                r = cli('запустить', str(p), *режим)
                self.assertNotEqual(r.returncode, 0)
                self.assertIn('неизвестное имя', r.stderr)

    def test_алиасы_и_разбор(self):
        r = cli('РАЗБЕРИ', 'слово', 'арбуз')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('арбуз', r.stdout)
        r = cli('ПРОВЕРЬ')
        self.assertIn('не хватает', r.stderr)
        self.assertNotIn('неизвестное дело', r.stderr)

    def test_ошибки_файлов(self):
        with tempfile.TemporaryDirectory() as d:
            missing = str(Path(d) / 'missing.lek')
            r = cli('запустить', missing)
            self.assertIn('файл не найден', r.stderr)
            p = Path(d) / 'binary.png'
            p.write_bytes(b'\x89PNG\xff')
            r = cli('байткод', str(p))
            self.assertIn('UTF-8', r.stderr)
            r = cli('запустить', str(p))
            self.assertIn('не программа Лексики', r.stderr)

    def test_беседа(self):
        r = cli('беседа', input='привет\nсказать привет\nпусть привет это привествие\nсказать привет\nЛЕКСИКА словарь человек\nлек выход\n')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('Здравствуйте!', r.stdout)
        self.assertIn('человек', r.stdout)
        self.assertIn('привествие', r.stdout)
        self.assertNotIn('ошибка', r.stdout)

    def test_установка_алиасов(self):
        # Проверяем реальный установщик без копирования дерева проекта.
        import runpy
        mod = runpy.run_path(str(ROOT / 'установить.lek'))
        with tempfile.TemporaryDirectory() as d:
            # Необязательная сборка ядра здесь не проверяется.
            from unittest.mock import patch
            with patch('shutil.which', return_value=None), patch.dict(mod['поставить'].__globals__, {'поставить_значок': lambda *args: None}):
                mod['поставить'](str(ROOT), d, ярлык=True)
            for name in ('Leksika', 'лексика', 'ЛЕКСИКА', 'лексус', 'ЛЕКСУС',
                         'лек', 'лекс', 'лексу', 'leksus', 'lek', 'leks'):
                path = Path(d) / name
                self.assertTrue(path.exists(), name)
                if os.name != 'nt':
                    r = subprocess.run([str(path), 'помощь'], capture_output=True, text=True)
                    self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == '__main__':
    unittest.main()
