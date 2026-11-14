"""Проверка пользовательских флагов нативных профилей (нужна сборка ядра)."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
CORE = ROOT / 'ядро' / ('лексус-ядро.exe' if os.name == 'nt' else 'лексус-ядро')


class ПрофилиCLI(unittest.TestCase):
    def test_флаги(self):
        self.assertTrue(CORE.is_file(), 'Сначала: make -C ядро native')
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'test.lek'
            for verb, flag in [('вкинуть', '--профиль=зумер'), ('заорать', '--профиль=18+')]:
                path.write_text(verb + ' 42.', encoding='utf-8')
                def run(*args):
                    return subprocess.run([str(CORE), *args], capture_output=True,
                                          text=True, encoding='utf-8')
                for args in [('выполнить', flag, str(path)),
                             ('--нативно', 'запустить', str(path), flag),
                             ('выполнить', '--профиль=18+', '--профиль=зумер', str(path))]:
                    result = run(*args)
                    self.assertEqual((result.returncode, result.stdout), (0, '42\n'), result.stderr)
                result = run('выполнить', str(path))
                self.assertNotEqual(result.returncode, 0)
                wrong = '--профиль=18+' if flag.endswith('зумер') else '--профиль=зумер'
                self.assertNotEqual(run('выполнить', wrong, str(path)).returncode, 0)
            result = run('выполнить', '--профиль=неизвестный', str(path))
            self.assertEqual(result.returncode, 2)
            self.assertIn('неизвестный профиль', result.stderr)
            self.assertEqual(run('выполнить', str(path), str(path)).returncode, 2)
            self.assertEqual(run('выполнить', '--профиль=зумер').returncode, 2)


if __name__ == '__main__':
    unittest.main()
