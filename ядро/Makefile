CC ?= cc
ifeq ($(OS),Windows_NT)
EXEEXT ?= .exe
else
EXEEXT ?=
endif
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
TARGET ?= лексус-ядро$(EXEEXT)
VALUE_TEST ?= значения-тест$(EXEEXT)
VM_TEST ?= машина-тест$(EXEEXT)
TLS_TOOL ?= лексус-сертификаты
OPENSSL_CFLAGS ?= $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS ?= $(shell pkg-config --libs openssl 2>/dev/null || printf '%s' '-lssl -lcrypto')
SOURCE = лексус.c значения.c словарь_индекс.c байткод.c машина.c
DICT_BUILDER = словарь-сборщик$(EXEEXT)
DICT_TEST = словарь-тест$(EXEEXT)
DICT_SOURCE = ../источники/russian-words/russian.txt
DICT_INDEX = русские_формы.lxd

.PHONY: all test dictionary dictionary-test tls-tools tls-test clean
all: $(TARGET) $(DICT_INDEX)

$(TARGET): $(SOURCE) значения.h байткод.h машина.h словарь_индекс.h словарь_формы.h морфология_окончания.h морфология_имён.h
	$(CC) $(CFLAGS) -o "$@" $(SOURCE)

$(VALUE_TEST): значения.c значения_проверка.c значения.h
	$(CC) $(CFLAGS) -o "$@" значения.c значения_проверка.c

$(VM_TEST): значения.c байткод.c машина.c машина_проверка.c значения.h байткод.h машина.h
	$(CC) $(CFLAGS) -o "$@" значения.c байткод.c машина.c машина_проверка.c

$(DICT_BUILDER): словарь_сборщик.c
	$(CC) $(CFLAGS) -o "$@" "$<"

dictionary: $(DICT_INDEX)

$(DICT_INDEX): $(DICT_BUILDER) $(DICT_SOURCE)
	./$(DICT_BUILDER) "$(DICT_SOURCE)" "$(DICT_INDEX)"

$(DICT_TEST): словарь_индекс.c словарь_индекс.h словарь_индекс_проверка.c
	$(CC) $(CFLAGS) -o "$@" словарь_индекс.c словарь_индекс_проверка.c

dictionary-test: $(DICT_TEST)
	./$(DICT_TEST)

test: $(TARGET) $(VALUE_TEST) $(DICT_TEST) $(VM_TEST)
	./$(TARGET) --self-test
	./$(VALUE_TEST)
	./$(DICT_TEST)
	./$(VM_TEST)

# Необязательный инструмент организаций. OpenSSL не входит в зависимости ядра.
tls-tools: $(TLS_TOOL)

$(TLS_TOOL): сертификаты.c
	$(CC) $(CFLAGS) $(OPENSSL_CFLAGS) -o "$@" "$<" $(OPENSSL_LIBS)

tls-test: $(TLS_TOOL)
	sh ./сертификаты_проверка.sh ./$(TLS_TOOL)

clean:
	rm -f "$(TARGET)" "$(TARGET).exe" "$(VALUE_TEST)" "$(VALUE_TEST).exe" "$(TLS_TOOL)" "$(TLS_TOOL).exe" "$(DICT_BUILDER)" "$(DICT_TEST)" "$(VM_TEST)"
