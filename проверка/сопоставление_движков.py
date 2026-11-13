"""Сравнение только общего поддерживаемого подмножества двух движков.

Сначала: make -C ядро native; затем: python3 проверка/сопоставление_движков.py
"""
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
NATIVE = ROOT / 'ядро' / ('лексус-ядро.exe' if os.name == 'nt' else 'лексус-ядро')


class Сопоставление(unittest.TestCase):
    def test_общий_синтаксис(self):
        if not NATIVE.is_file():
            self.fail('Сначала соберите нативное ядро: make -C ядро native')
        cases = [
            ('пускай счёт это 2. что (счёт плюс 3) умножить на 4', '20\n'),
            ('пусть счёт равно 42. скажи счёт', '42\n'),
            ('пусть счёт это 42. выведи счёт', '42\n'),
            ('пусть счёт это 42. говори счёт', '42\n'),
            ('когда 2 больше 1 тогда\nпокажи 42.\nконец когда', '42\n'),
            ('сказать речь Привет конец речи', 'Привет\n'),
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'программа.lek'
            for source, expected in cases:
                with self.subTest(source=source):
                    path.write_text(source, encoding='utf-8')
                    for command in ([str(NATIVE), 'выполнить', str(path)],
                                    [sys.executable, str(ROOT / 'лексус.lek'), 'запустить', str(path)],
                                    [sys.executable, str(ROOT / 'лексус.lek'), 'запустить', str(path), '--дерево']):
                        result = subprocess.run(command, cwd=ROOT, capture_output=True,
                                                text=True, encoding='utf-8')
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(result.stdout, expected)
            path.write_text('что неизвестное', encoding='utf-8')
            for command in ([str(NATIVE), 'выполнить', str(path)],
                            [sys.executable, str(ROOT / 'лексус.lek'), 'запустить', str(path)]):
                self.assertNotEqual(subprocess.run(command, cwd=ROOT, capture_output=True).returncode, 0)

    @unittest.skipIf(os.name == 'nt', 'проверка Unix PATH')
    def test_нативный_запуск_без_python_в_path(self):
        if not NATIVE.is_file():
            self.fail('Сначала соберите нативное ядро: make -C ядро native')
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            os.symlink(shutil.which('dirname'), temp / 'dirname')
            script = temp / 'пример.lek'
            script.write_text('скажи 42', encoding='utf-8')
            env = dict(os.environ, PATH=directory)
            result = subprocess.run([str(ROOT / 'Leksika'), '--нативно', 'запустить', str(script)],
                                    env=env, capture_output=True, text=True, encoding='utf-8')
            self.assertEqual((result.returncode, result.stdout), (0, '42\n'), result.stderr)
            script.write_text('правило недоступно. конец правила.', encoding='utf-8')
            result = subprocess.run([str(ROOT / 'Leksika'), '--нативно', 'запустить', str(script)],
                                    env=env, capture_output=True, text=True, encoding='utf-8')
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('Python', result.stderr)


if __name__ == '__main__':
    unittest.main()
