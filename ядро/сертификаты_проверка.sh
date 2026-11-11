#!/bin/sh
set -eu

TOOL=${1:-./лексус-сертификаты}
TMP_DIR=$(mktemp -d /tmp/lexus-tls-test.XXXXXX)
SERVER_PID=""

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT HUP INT TERM

command -v openssl >/dev/null 2>&1 || {
    echo "Нужен OpenSSL для тестов TLS." >&2
    exit 2
}

echo "Создание временного тестового центра сертификации..."
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$TMP_DIR/root.key" -out "$TMP_DIR/root.pem" \
    -days 2 -subj "/CN=Lexica Test Root" >/dev/null 2>&1

openssl req -new -newkey rsa:2048 -nodes \
    -keyout "$TMP_DIR/server.key" -out "$TMP_DIR/server.csr" \
    -subj "/CN=localhost" >/dev/null 2>&1

cat > "$TMP_DIR/server.ext" <<'EOF'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost
EOF

openssl x509 -req -in "$TMP_DIR/server.csr" \
    -CA "$TMP_DIR/root.pem" -CAkey "$TMP_DIR/root.key" -CAcreateserial \
    -out "$TMP_DIR/server.pem" -days 1 -sha256 \
    -extfile "$TMP_DIR/server.ext" >/dev/null 2>&1

echo "Проверка X.509-цепочки..."
"$TOOL" проверить "$TMP_DIR/server.pem" "$TMP_DIR/root.pem"

echo "Проверка диагностики сертификата..."
"$TOOL" инфо "$TMP_DIR/server.pem" >/dev/null

openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$TMP_DIR/wrong.key" -out "$TMP_DIR/wrong.pem" \
    -days 2 -subj "/CN=Wrong Test Root" >/dev/null 2>&1

if "$TOOL" проверить "$TMP_DIR/server.pem" "$TMP_DIR/wrong.pem" >/dev/null 2>&1; then
    echo "ОШИБКА: неверный корневой сертификат был принят." >&2
    exit 1
fi

PORT=$((20000 + ($$ % 10000)))
openssl s_server -quiet -accept "$PORT" \
    -cert "$TMP_DIR/server.pem" -key "$TMP_DIR/server.key" -www \
    >"$TMP_DIR/server.log" 2>&1 &
SERVER_PID=$!

echo "Проверка TLS и имени узла на локальном тестовом сервере..."
ATTEMPT=0
while ! "$TOOL" tls localhost "$PORT" --корень "$TMP_DIR/root.pem" \
        >"$TMP_DIR/client.log" 2>&1; do
    ATTEMPT=$((ATTEMPT + 1))
    if [ "$ATTEMPT" -ge 15 ]; then
        cat "$TMP_DIR/client.log" >&2
        echo "ОШИБКА: локальная TLS-проверка не прошла." >&2
        exit 1
    fi
    sleep 0.2
done
cat "$TMP_DIR/client.log"

echo "Проверки сертификатного инструмента пройдены."
