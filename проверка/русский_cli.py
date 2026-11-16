#!/usr/bin/env python3
"""Проверка, что командный интерфейс говорит по-русски.

Проверяются два обещания:
1. в справке нет английских команд и флагов — пользователь не должен их знать;
2. старые английские написания всё ещё работают как скрытые псевдонимы.

Запуск: python3 проверка/русский_cli.py
"""

import re
import subprocess
import sys
import unittest
from pathlib import Path

КОРЕНЬ = Path(__file__).resolve().parent.parent
ЯДРО = КОРЕНЬ / "ядро" / "лексус-ядро"
ЗАПУСКАТЕЛЬ = КОРЕНЬ / "Leksika"

# Английское слово после дефисов: --version, --self-test, --lex.
АНГЛИЙСКИЙ_ФЛАГ = re.compile(r"--[A-Za-z][A-Za-z-]*")
# Слова справки, которые обязаны быть русскими.
РУССКИЕ_КОМАНДЫ = ("версия", "самопроверка", "проверить", "разобрать", "склонять")


def запустить(команда):
    return subprocess.run(команда, capture_output=True, text=True, encoding="utf-8")


class РусскийCLI(unittest.TestCase):
    def setUp(self):
        self.assertTrue(ЯДРО.is_file(), "Сначала: make -C ядро core")

    def test_ядра_справка_без_английского(self):
        результат = запустить([str(ЯДРО)])
        справка = результат.stdout
        for команда in РУССКИЕ_КОМАНДЫ:
            self.assertIn(команда, справка)
        self.assertEqual(АНГЛИЙСКИЙ_ФЛАГ.findall(справка), [],
                         "в справке ядра остались английские флаги")

    def test_ядра_русские_команды(self):
        self.assertEqual(запустить([str(ЯДРО), "версия"]).stdout.strip()[:6], "Лексус")
        self.assertEqual(запустить([str(ЯДРО), "самопроверка"]).returncode, 0)
        # «нативно» снимается ядром так же, как английское --нативно.
        self.assertEqual(запустить([str(ЯДРО), "нативно", "версия"]).returncode, 0)

    def test_старые_флаги_работают(self):
        self.assertEqual(запустить([str(ЯДРО), "--version"]).returncode, 0)
        self.assertEqual(запустить([str(ЯДРО), "--self-test"]).returncode, 0)

    def test_запускатель_нативно(self):
        результат = запустить([str(ЗАПУСКАТЕЛЬ), "нативно", "версия"])
        self.assertEqual(результат.returncode, 0, результат.stderr)
        само = запустить([str(ЗАПУСКАТЕЛЬ), "нативно", "самопроверка"])
        self.assertEqual(само.returncode, 0, само.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
