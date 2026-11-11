CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
TARGET ?= лексус-ядро
VALUE_TEST ?= значения-тест
TLS_TOOL ?= лексус-сертификаты
OPENSSL_CFLAGS ?= $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS ?= $(shell pkg-config --libs openssl 2>/dev/null || printf '%s' '-lssl -lcrypto')
SOURCE = лексус.c

.PHONY: all test tls-tools tls-test clean
all: $(TARGET)

$(TARGET): $(SOURCE) словарь_формы.h морфология_окончания.h морфология_имён.h
	$(CC) $(CFLAGS) -o "$@" "$<"

$(VALUE_TEST): значения.c значения_проверка.c значения.h
	$(CC) $(CFLAGS) -o "$@" значения.c значения_проверка.c

test: $(TARGET) $(VALUE_TEST)
	./$(TARGET) --self-test
	./$(VALUE_TEST)

# Необязательный инструмент организаций. OpenSSL не входит в зависимости ядра.
tls-tools: $(TLS_TOOL)

$(TLS_TOOL): сертификаты.c
	$(CC) $(CFLAGS) $(OPENSSL_CFLAGS) -o "$@" "$<" $(OPENSSL_LIBS)

tls-test: $(TLS_TOOL)
	sh ./сертификаты_проверка.sh ./$(TLS_TOOL)

clean:
	rm -f "$(TARGET)" "$(TARGET).exe" "$(VALUE_TEST)" "$(VALUE_TEST).exe" "$(TLS_TOOL)" "$(TLS_TOOL).exe"
