CC ?= cc
ifeq ($(OS),Windows_NT)
EXEEXT ?= .exe
NATIVE_LIBS = -lshell32
else
EXEEXT ?=
endif
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8
TARGET ?= лексус-ядро$(EXEEXT)
VALUE_TEST ?= значения-тест$(EXEEXT)
VM_TEST ?= машина-тест$(EXEEXT)
COMPILER_TEST ?= компилятор-тест$(EXEEXT)
CONTRACT_TEST ?= контракт-тест$(EXEEXT)
BYTECODE_TEST ?= байткод-формат-тест$(EXEEXT)
TLS_TOOL ?= лексус-сертификаты
OPENSSL_CFLAGS ?= $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS ?= $(shell pkg-config --libs openssl 2>/dev/null || printf '%s' '-lssl -lcrypto')
SOURCE = лексус.c значения.c словарь_индекс.c байткод.c байткод_формат.c машина.c компилятор.c морфология.c
DICT_BUILDER = словарь-сборщик$(EXEEXT)
DICT_TEST = словарь-тест$(EXEEXT)
# Внешний CP1251-список — необязательный источник подсказок. Ядро, морфология
# и все тесты работают без него: цель native его не трогает.
DICT_SOURCE = ../источники/russian-words/russian.txt
DICT_INDEX = русские_формы.lxd

.PHONY: all native core test sanitize-test dictionary dictionary-test compiler-test contract-test bytecode-test morphology-test tls-tools tls-test clean
# all собирает ещё и словарь подсказок из внешнего источника; core — только ядро.
all: $(TARGET) $(DICT_INDEX)

core: $(TARGET)

# native — старое имя цели core; оставлено, чтобы существующие скрипты работали.
native: core

$(TARGET): $(SOURCE) значения.h байткод.h машина.h компилятор.h словарь_индекс.h словарь_формы.h морфология_окончания.h морфология_имён.h морфология.h
	$(CC) $(CFLAGS) -o "$@" $(SOURCE) $(NATIVE_LIBS)

$(VALUE_TEST): значения.c значения_проверка.c значения.h
	$(CC) $(CFLAGS) -o "$@" значения.c значения_проверка.c

$(VM_TEST): значения.c байткод.c машина.c машина_проверка.c значения.h байткод.h машина.h
	$(CC) $(CFLAGS) -o "$@" значения.c байткод.c машина.c машина_проверка.c

$(COMPILER_TEST): значения.c байткод.c машина.c компилятор.c морфология.c компилятор_проверка.c значения.h байткод.h машина.h компилятор.h морфология.h
	$(CC) $(CFLAGS) -o "$@" значения.c байткод.c машина.c компилятор.c морфология.c компилятор_проверка.c

compiler-test: $(COMPILER_TEST)

$(CONTRACT_TEST): значения.c байткод.c машина.c компилятор.c морфология.c контракт_проверка.c значения.h байткод.h машина.h компилятор.h морфология.h
	$(CC) $(CFLAGS) -o "$@" значения.c байткод.c машина.c компилятор.c морфология.c контракт_проверка.c

МОРФОЛОГИЯ_TEST ?= морфология-тест$(EXEEXT)

$(МОРФОЛОГИЯ_TEST): морфология.c морфология_проверка.c морфология.h
	$(CC) $(CFLAGS) -o "$@" морфология.c морфология_проверка.c

morphology-test: $(МОРФОЛОГИЯ_TEST)
	./$(МОРФОЛОГИЯ_TEST)

contract-test: $(CONTRACT_TEST)
	./$(CONTRACT_TEST)
	./$(МОРФОЛОГИЯ_TEST)

$(BYTECODE_TEST): значения.c байткод.c байткод_формат.c машина.c компилятор.c морфология.c байткод_формат_проверка.c значения.h байткод.h машина.h компилятор.h морфология.h
	$(CC) $(CFLAGS) -o "$@" значения.c байткод.c байткод_формат.c машина.c компилятор.c морфология.c байткод_формат_проверка.c

bytecode-test: $(BYTECODE_TEST)
	./$(BYTECODE_TEST)

$(DICT_BUILDER): словарь_сборщик.c
	$(CC) $(CFLAGS) -o "$@" "$<"

dictionary: $(DICT_INDEX)

$(DICT_INDEX): $(DICT_BUILDER) $(DICT_SOURCE)
	./$(DICT_BUILDER) "$(DICT_SOURCE)" "$(DICT_INDEX)"

$(DICT_TEST): словарь_индекс.c словарь_индекс.h словарь_индекс_проверка.c
	$(CC) $(CFLAGS) -o "$@" словарь_индекс.c словарь_индекс_проверка.c

dictionary-test: $(DICT_TEST)
	./$(DICT_TEST)

test: $(TARGET) $(VALUE_TEST) $(DICT_TEST) $(VM_TEST) $(COMPILER_TEST) $(CONTRACT_TEST) $(МОРФОЛОГИЯ_TEST) $(BYTECODE_TEST)
	./$(TARGET) самопроверка
	./$(VALUE_TEST)
	./$(DICT_TEST)
	./$(VM_TEST)
	./$(COMPILER_TEST)
	./$(CONTRACT_TEST)
	./$(МОРФОЛОГИЯ_TEST)
	./$(BYTECODE_TEST)

# Полная проверка с AddressSanitizer и UndefinedBehaviorSanitizer (GCC/Clang).
sanitize-test:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 -fsanitize=address,undefined -fno-omit-frame-pointer' all test

# Необязательный инструмент организаций. OpenSSL не входит в зависимости ядра.
tls-tools: $(TLS_TOOL)

$(TLS_TOOL): сертификаты.c
	$(CC) $(CFLAGS) $(OPENSSL_CFLAGS) -o "$@" "$<" $(OPENSSL_LIBS)

tls-test: $(TLS_TOOL)
	sh ./сертификаты_проверка.sh ./$(TLS_TOOL)

clean:
	rm -f "$(DICT_INDEX)" "$(TARGET)" "$(TARGET).exe" "$(VALUE_TEST)" "$(VALUE_TEST).exe" "$(TLS_TOOL)" "$(TLS_TOOL).exe" "$(DICT_BUILDER)" "$(DICT_TEST)" "$(DICT_TEST).exe" "$(DICT_BUILDER)" "$(DICT_BUILDER).exe" "$(VM_TEST)" "$(VM_TEST).exe" "$(COMPILER_TEST)" "$(COMPILER_TEST).exe" "$(CONTRACT_TEST)" "$(CONTRACT_TEST).exe" "$(BYTECODE_TEST)" "$(BYTECODE_TEST).exe" "$(МОРФОЛОГИЯ_TEST)" "$(МОРФОЛОГИЯ_TEST).exe"
