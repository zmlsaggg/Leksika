# Leksika — a Russian-language programming language

**Leksika** is an experimental programming language whose keywords, identifiers and core syntax are designed around Russian grammar. Its distinctive feature is case-based grammatical relationships: the grammatical case of a word helps express its role in an operation.

**Lexus** is the language engine and toolchain. Source programs use the `.lek` extension.

- Official website: https://zmlsaggg.github.io/Leksika/
- Browser playground: https://zmlsaggg.github.io/Leksika/playground.html
- Public project repository: https://github.com/zmlsaggg/Leksika
- Development repository: https://github.com/zmlsaggg/Leksikadev

## Try a small example

```text
пусть сумма равно пяти.
положить десять в сумму.
прибавить два к сумме.
сказать речь Сумма равна (сумма) конец речи.
```

The browser playground runs an educational subset of the language locally in a modern browser. It is not the full desktop engine.

## Project status and implementation

Leksika has its own syntax, morphology rules, examples and documentation. The desktop runtime is still in transition: parts of the current engine use Python 3.8+ and some native C11 bootstrap components are developed separately. The complete execution pipeline has **not** yet become independent of Python. The project documents this limitation openly and tracks the migration in its runtime plan.

The language supports a word-based form and a compatible notation with familiar punctuation. This compatible notation is not a complete Python, C or C++ parser.

## Development

See the Russian [README](README.md) for installation instructions, examples and the current feature list. Contributions are welcome; read [CONTRIBUTING.md](CONTRIBUTING.md) first.

## Security and licensing

Please report vulnerabilities privately as described in [SECURITY.md](SECURITY.md). Do not publish unpatched security issues in public discussions.

Leksika is distributed under the [MIT License](LICENSE).
