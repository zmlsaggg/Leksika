# Leksika — a Russian-first programming language

**Leksika** is an experimental programming language whose keywords, identifiers and core syntax are designed around Russian grammar. Its distinctive feature is case-based grammatical relationships: the grammatical case of a word helps express its role in an operation.

**Lexus** is the language engine and toolchain. Source programs use the `.lek` extension.

- Official website: https://leksika.gleeze.com/
- Browser playground: https://leksika.gleeze.com/playground.html
- Public project repository: https://github.com/zmlsaggg/Leksika
- Development repository: https://github.com/zmlsaggg/Leksikadev

## Try a small example

```text
пусть сумма равно пяти.
положить десять в сумму.
прибавить два к сумме.
сказать речь Сумма равна (сумма) конец речи.
```

The [browser playground](https://leksika.gleeze.com/playground.html) runs an educational subset of the language in a modern browser. It includes two AI modes: a code-focused assistant that reviews syntax and suggests changes for approval, and a general chat assistant.

Compatible public Hugging Face models can run locally in the browser through WebGPU or WebAssembly when supported. The playground can also connect to user-configured OpenAI-compatible, Anthropic or Gemini APIs. When an external API is selected, the provider receives the prompt and code sent to it. The playground is not the full desktop engine.

## Project status and implementation

Leksika has its own syntax, morphology rules, examples and documentation. The desktop runtime is still in transition: parts of the current engine use Python 3.8+ and some native C11 bootstrap components are developed separately. The complete execution pipeline has **not** yet become independent of Python. The project documents this limitation openly and tracks the migration in its runtime plan.

The language supports a word-based form and a compatible notation with familiar punctuation. This compatible notation is not a complete Python, C or C++ parser.

## Native runtime

A native compiler and register-based virtual machine are being developed alongside the transition desktop runtime. The native subset includes exact decimal arithmetic, Russian number words, conditionals, loops, lists and a verified bytecode execution path. The desktop runtime still uses Python 3.8+ for parts of the full language; complete Python independence and self-hosting remain future milestones.

The project also builds a binary Russian word-form index used by the native tooling and browser editor suggestions.

Technical notes:
- [Native compiler](документы/ПЕРВЫЙ_НАТИВНЫЙ_КОМПИЛЯТОР.md)
- [Online playground](документы/ОНЛАЙН_ПЕСОЧНИЦА.md)
- [Runtime migration plan](ПЛАН_ПЕРЕНОСА_РАНТАЙМА.md)

## Development

See the Russian [README](README.md) for installation instructions, examples and the current feature list. Contributions are welcome; read [CONTRIBUTING.md](CONTRIBUTING.md) first.

## Security and licensing

Please report vulnerabilities privately as described in [SECURITY.md](SECURITY.md). Do not publish unpatched security issues in public discussions.

Leksika is distributed under the [MIT License](LICENSE).
