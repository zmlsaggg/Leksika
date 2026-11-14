import hashlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.environ.get('LEKSIKA_MODEL_DIR', str(Path(__file__).resolve().parent.parent / 'лексика')))
import скачать_модели as loader


class ЗагрузкаМоделей(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = b'GGUF test data'
        self.model = {'имя': 'test.gguf', 'ссылка': 'https://example.test/model',
                      'размер': len(self.data), 'sha256': hashlib.sha256(self.data).hexdigest()}

    def download(self, data):
        return loader.скачать(self.model, self.root, lambda _: io.BytesIO(data), lambda _: None)

    def test_успех_и_повторное_использование(self):
        path = self.download(self.data)
        self.assertEqual(path.read_bytes(), self.data)
        loader.скачать(self.model, self.root, lambda _: self.fail('лишняя загрузка'), lambda _: None)

    def test_неверный_хеш_не_заменяет_старый_файл(self):
        target = self.root / 'test.gguf'
        target.write_bytes(b'original')
        with self.assertRaises(ValueError):
            self.download(b'x' * len(self.data))
        self.assertEqual(target.read_bytes(), b'original')
        self.assertEqual(list(self.root.glob('*.part')), [])

    def test_неполный_и_слишком_большой_файл(self):
        for data in (self.data[:-1], self.data + b'x'):
            with self.subTest(data=data), self.assertRaises(ValueError):
                self.download(data)
            self.assertEqual(list(self.root.iterdir()), [])

    def test_ошибка_сети_удаляет_временный_файл(self):
        def broken(_):
            raise OSError('network')
        with self.assertRaises(OSError):
            loader.скачать(self.model, self.root, broken, lambda _: None)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_http_запрещён(self):
        with self.assertRaises(ValueError):
            loader.открыть('http://example.test/')
        handler = loader.ТолькоHTTPS()
        request = loader.urllib.request.Request('https://example.test/')
        with self.assertRaises(ValueError):
            handler.redirect_request(request, None, 302, '', {}, 'http://example.test/')


if __name__ == '__main__':
    unittest.main()
