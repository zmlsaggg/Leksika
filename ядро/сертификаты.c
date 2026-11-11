/*
 * Необязательный инструмент корпоративной TLS-диагностики Лексуса.
 * Требует OpenSSL 3.x. Основная сборка ядра от этой зависимости не зависит.
 *
 * Команды:
 *   лексус-сертификаты инфо сертификат.cer
 *   лексус-сертификаты проверить сервер.cer корень.cer [промежуточный.cer]
 *   лексус-сертификаты p12 клиент.p12
 *   лексус-сертификаты tls api.example.ru [443] [--корень ca.pem]
 *                                      [--клиент-p12 клиент.p12]
 *
 * Инструмент не устанавливает корневые сертификаты в ОС и не экспортирует
 * закрытый ключ из PKCS#12. Пароль читается без отображения и не передаётся
 * через аргументы командной строки.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/provider.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#define ДЛИНА_ПАРОЛЯ 1024
#define РАЗМЕР_ОШИБКИ 256

static void показать_ошибку_openssl(const char *контекст)
{
    unsigned long код;
    fprintf(stderr, "Ошибка: %s\n", контекст);
    while ((код = ERR_get_error()) != 0) {
        char строка[256];
        ERR_error_string_n(код, строка, sizeof(строка));
        fprintf(stderr, "  OpenSSL: %s\n", строка);
    }
}

static int прочитать_пароль(char *буфер, size_t размер)
{
    size_t длина = 0;
    if (!буфер || размер < 2) return 0;
    fprintf(stderr, "Пароль контейнера PKCS#12 (ввод скрыт): ");
    fflush(stderr);
#ifdef _WIN32
    for (;;) {
        int знак = _getch();
        if (знак == '\r' || знак == '\n') break;
        if (знак == 3) {
            fprintf(stderr, "\nВвод отменён.\n");
            OPENSSL_cleanse(буфер, размер);
            return 0;
        }
        if (знак == 8 || знак == 127) {
            if (длина) --длина;
            continue;
        }
        if (знак >= 32 && знак <= 126 && длина + 1 < размер)
            буфер[длина++] = (char)знак;
    }
#else
    struct termios старые, новые;
    if (tcgetattr(STDIN_FILENO, &старые) != 0) {
        fprintf(stderr, "\nНужен интерактивный терминал для безопасного ввода пароля.\n");
        return 0;
    }
    новые = старые;
    новые.c_lflag &= (tcflag_t)~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &новые) != 0) return 0;
    for (;;) {
        int знак = getchar();
        if (знак == EOF || знак == '\n' || знак == '\r') break;
        if (знак == 127 || знак == 8) {
            if (длина) --длина;
            continue;
        }
        if (знак >= 32 && знак <= 126 && длина + 1 < размер)
            буфер[длина++] = (char)знак;
    }
    (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &старые);
#endif
    буфер[длина] = '\0';
    fprintf(stderr, "\n");
    return 1;
}

static X509 *прочитать_сертификат(const char *путь)
{
    FILE *файл;
    X509 *сертификат = NULL;
    if (!путь || !(файл = fopen(путь, "rb"))) {
        fprintf(stderr, "Не удалось открыть сертификат: %s\n", путь ? путь : "(путь не задан)");
        return NULL;
    }
    сертификат = PEM_read_X509(файл, NULL, NULL, NULL);
    if (!сертификат) {
        ERR_clear_error();
        rewind(файл);
        сертификат = d2i_X509_fp(файл, NULL);
    }
    fclose(файл);
    if (!сертификат) показать_ошибку_openssl("файл не является читаемым X.509 PEM/DER сертификатом");
    return сертификат;
}

static int напечатать_имя(const char *метка, X509_NAME *имя)
{
    BIO *вывод = BIO_new(BIO_s_mem());
    char *данные = NULL;
    long длина;
    if (!вывод || !имя) { BIO_free(вывод); return 0; }
    if (X509_NAME_print_ex(вывод, имя, 0, XN_FLAG_RFC2253) < 0) {
        BIO_free(вывод);
        return 0;
    }
    длина = BIO_get_mem_data(вывод, &данные);
    if (длина < 0 || !данные) { BIO_free(вывод); return 0; }
    printf("%s: %.*s\n", метка, (int)длина, данные);
    BIO_free(вывод);
    return 1;
}

static void напечатать_дату(const char *метка, const ASN1_TIME *время)
{
    BIO *вывод = BIO_new(BIO_s_mem());
    char *данные = NULL;
    long длина;
    if (!вывод) return;
    if (ASN1_TIME_print(вывод, время) == 1) {
        длина = BIO_get_mem_data(вывод, &данные);
        if (длина > 0 && данные) printf("%s: %.*s\n", метка, (int)длина, данные);
    }
    BIO_free(вывод);
}

static int напечатать_отпечаток(X509 *сертификат)
{
    unsigned char отпечаток[EVP_MAX_MD_SIZE];
    unsigned длина = 0;
    if (X509_digest(сертификат, EVP_sha256(), отпечаток, &длина) != 1) return 0;
    printf("SHA-256: ");
    for (unsigned i = 0; i < длина; ++i)
        printf("%s%02X", i ? ":" : "", отпечаток[i]);
    printf("\n");
    OPENSSL_cleanse(отпечаток, sizeof(отпечаток));
    return 1;
}

static void напечатать_алгоритмы(X509 *сертификат)
{
    int nid = X509_get_signature_nid(сертификат);
    const char *подпись = OBJ_nid2ln(nid);
    EVP_PKEY *ключ = X509_get_pubkey(сертификат);
    printf("Алгоритм подписи: %s\n", подпись ? подпись : "неизвестен");
    if (ключ) {
        int тип = EVP_PKEY_base_id(ключ);
        const char *имя = OBJ_nid2ln(EVP_PKEY_type(тип));
        printf("Открытый ключ: %s, %d бит\n",
               имя ? имя : "неизвестный алгоритм", EVP_PKEY_get_bits(ключ));
        EVP_PKEY_free(ключ);
    }
}

static int команда_инфо(const char *путь)
{
    X509 *сертификат = прочитать_сертификат(путь);
    int истёк, ещё_не_действует;
    if (!сертификат) return 2;
    printf("Сертификат: %s\n", путь);
    (void)напечатать_имя("Владелец", X509_get_subject_name(сертификат));
    (void)напечатать_имя("Издатель", X509_get_issuer_name(сертификат));
    напечатать_дату("Действует с", X509_get0_notBefore(сертификат));
    напечатать_дату("Действует до", X509_get0_notAfter(сертификат));
    напечатать_алгоритмы(сертификат);
    printf("CA-сертификат: %s\n", X509_check_ca(сертификат) > 0 ? "да" : "нет");
    if (!напечатать_отпечаток(сертификат)) {
        X509_free(сертификат);
        показать_ошибку_openssl("не удалось вычислить отпечаток");
        return 2;
    }
    истёк = X509_cmp_current_time(X509_get0_notAfter(сертификат)) < 0;
    ещё_не_действует = X509_cmp_current_time(X509_get0_notBefore(сертификат)) > 0;
    if (истёк) printf("Статус срока: ИСТЁК\n");
    else if (ещё_не_действует) printf("Статус срока: ЕЩЁ НЕ НАСТУПИЛ\n");
    else printf("Статус срока: действует по текущим системным часам\n");
    X509_free(сертификат);
    return (истёк || ещё_не_действует) ? 1 : 0;
}

static int команда_проверить(const char *путь_листа, const char *путь_корня,
                             const char *путь_промежуточного)
{
    X509 *лист = NULL, *корень = NULL, *промежуточный = NULL;
    X509_STORE *хранилище = NULL;
    X509_STORE_CTX *контекст = NULL;
    STACK_OF(X509) *цепочка = NULL;
    int итог = 2, проверка;
    лист = прочитать_сертификат(путь_листа);
    корень = прочитать_сертификат(путь_корня);
    if (!лист || !корень) goto выход;
    if (X509_check_ca(корень) <= 0) {
        fprintf(stderr, "Указанный корневой файл не помечен как CA-сертификат.\n");
        goto выход;
    }
    хранилище = X509_STORE_new();
    контекст = X509_STORE_CTX_new();
    if (!хранилище || !контекст) goto память;
    if (X509_STORE_add_cert(хранилище, корень) != 1) {
        ERR_clear_error();
        fprintf(stderr, "Не удалось добавить корневой сертификат в проверочное хранилище.\n");
        goto выход;
    }
    if (путь_промежуточного) {
        промежуточный = прочитать_сертификат(путь_промежуточного);
        if (!промежуточный) goto выход;
        if (X509_check_ca(промежуточный) <= 0) {
            fprintf(stderr, "Промежуточный сертификат не является CA-сертификатом.\n");
            goto выход;
        }
        цепочка = sk_X509_new_null();
        if (!цепочка || !sk_X509_push(цепочка, промежуточный)) goto память;
    }
    if (X509_STORE_CTX_init(контекст, хранилище, лист, цепочка) != 1) goto память;
    проверка = X509_verify_cert(контекст);
    if (проверка == 1) {
        printf("Цепочка доверия подтверждена.\n");
        (void)напечатать_имя("Владелец", X509_get_subject_name(лист));
        (void)напечатать_имя("Корень доверия", X509_get_subject_name(корень));
        итог = 0;
    } else {
        int код = X509_STORE_CTX_get_error(контекст);
        fprintf(stderr, "Цепочка НЕ подтверждена: %s (код %d).\n",
                X509_verify_cert_error_string(код), код);
        итог = 1;
    }
    goto выход;
память:
    показать_ошибку_openssl("не удалось выделить память для проверки цепочки");
выход:
    X509_STORE_CTX_free(контекст);
    sk_X509_free(цепочка);
    X509_STORE_free(хранилище);
    X509_free(промежуточный);
    X509_free(корень);
    X509_free(лист);
    return итог;
}

static PKCS12 *прочитать_p12(const char *путь)
{
    FILE *файл = fopen(путь, "rb");
    PKCS12 *контейнер;
    if (!файл) {
        fprintf(stderr, "Не удалось открыть PKCS#12: %s\n", путь);
        return NULL;
    }
    контейнер = d2i_PKCS12_fp(файл, NULL);
    fclose(файл);
    if (!контейнер) показать_ошибку_openssl("не удалось разобрать PKCS#12");
    return контейнер;
}

static int разобрать_p12(const char *путь, EVP_PKEY **ключ, X509 **сертификат,
                         STACK_OF(X509) **цепочка)
{
    PKCS12 *контейнер = NULL;
    OSSL_PROVIDER *провайдер_по_умолчанию = NULL, *legacy_провайдер = NULL;
    char пароль[ДЛИНА_ПАРОЛЯ];
    int ok = 0;
    *ключ = NULL; *сертификат = NULL; *цепочка = NULL;
    memset(пароль, 0, sizeof(пароль));
    /* Старые PFX могут использовать RC2 для упаковки контейнера.
       Legacy включается только на время импорта P12, не для TLS-сеанса. */
    провайдер_по_умолчанию = OSSL_PROVIDER_load(NULL, "default");
    legacy_провайдер = OSSL_PROVIDER_load(NULL, "legacy");
    if (!провайдер_по_умолчанию) {
        показать_ошибку_openssl("не удалось загрузить основной OpenSSL provider");
        goto выход;
    }
    ERR_clear_error();
    контейнер = прочитать_p12(путь);
    if (!контейнер) goto выход;
    if (!прочитать_пароль(пароль, sizeof(пароль))) goto выход;
    if (PKCS12_verify_mac(контейнер, пароль, (int)strlen(пароль)) != 1) {
        fprintf(stderr, "Пароль неверен или контрольная сумма контейнера не подтверждена.\n");
        ERR_clear_error();
        goto выход;
    }
    if (PKCS12_parse(контейнер, пароль, ключ, сертификат, цепочка) != 1) {
        показать_ошибку_openssl("не удалось открыть содержимое PKCS#12");
        goto выход;
    }
    if (!*ключ || !*сертификат) {
        fprintf(stderr, "В контейнере нет одновременно клиентского сертификата и закрытого ключа.\n");
        goto выход;
    }
    if (X509_check_private_key(*сертификат, *ключ) != 1) {
        показать_ошибку_openssl("закрытый ключ не соответствует сертификату");
        goto выход;
    }
    ok = 1;
выход:
    OPENSSL_cleanse(пароль, sizeof(пароль));
    PKCS12_free(контейнер);
    if (legacy_провайдер) OSSL_PROVIDER_unload(legacy_провайдер);
    if (провайдер_по_умолчанию) OSSL_PROVIDER_unload(провайдер_по_умолчанию);
    if (!ok) {
        EVP_PKEY_free(*ключ); *ключ = NULL;
        X509_free(*сертификат); *сертификат = NULL;
        sk_X509_pop_free(*цепочка, X509_free); *цепочка = NULL;
    }
    return ok;
}

static int команда_p12(const char *путь)
{
    EVP_PKEY *ключ = NULL;
    X509 *сертификат = NULL;
    STACK_OF(X509) *цепочка = NULL;
    int истёк, не_начался, код = 2;
    if (!разобрать_p12(путь, &ключ, &сертификат, &цепочка)) goto выход;
    printf("PKCS#12: контейнер открыт, MAC подтверждён, ключ соответствует сертификату.\n");
    (void)напечатать_имя("Владелец", X509_get_subject_name(сертификат));
    (void)напечатать_имя("Издатель", X509_get_issuer_name(сертификат));
    напечатать_дату("Действует с", X509_get0_notBefore(сертификат));
    напечатать_дату("Действует до", X509_get0_notAfter(сертификат));
    напечатать_алгоритмы(сертификат);
    printf("Сертификатов цепочки в контейнере: %d\n", цепочка ? sk_X509_num(цепочка) : 0);
    истёк = X509_cmp_current_time(X509_get0_notAfter(сертификат)) < 0;
    не_начался = X509_cmp_current_time(X509_get0_notBefore(сертификат)) > 0;
    if (истёк || не_начался) {
        fprintf(stderr, "Предупреждение: сертификат вне срока действия.\n");
        код = 1;
    } else код = 0;
выход:
    EVP_PKEY_free(ключ);
    X509_free(сертификат);
    sk_X509_pop_free(цепочка, X509_free);
    return код;
}

static int подключить_p12(SSL_CTX *ctx, const char *путь)
{
    EVP_PKEY *ключ = NULL;
    X509 *сертификат = NULL;
    STACK_OF(X509) *цепочка = NULL;
    int ok = 0;
    if (!разобрать_p12(путь, &ключ, &сертификат, &цепочка)) return 0;
    if (X509_cmp_current_time(X509_get0_notAfter(сертификат)) < 0 ||
        X509_cmp_current_time(X509_get0_notBefore(сертификат)) > 0) {
        fprintf(stderr, "Клиентский сертификат вне срока действия.\n");
        goto выход;
    }
    if (SSL_CTX_use_certificate(ctx, сертификат) != 1 ||
        SSL_CTX_use_PrivateKey(ctx, ключ) != 1 ||
        SSL_CTX_check_private_key(ctx) != 1) {
        показать_ошибку_openssl("не удалось подключить клиентский сертификат");
        goto выход;
    }
    if (цепочка) {
        for (int i = 0; i < sk_X509_num(цепочка); ++i) {
            X509 *с = sk_X509_value(цепочка, i);
            if (SSL_CTX_add1_chain_cert(ctx, с) != 1) {
                показать_ошибку_openssl("не удалось подключить промежуточный сертификат клиента");
                goto выход;
            }
        }
    }
    printf("Клиентский сертификат подключён к TLS-контексту без экспорта закрытого ключа.\n");
    (void)напечатать_имя("Клиент", X509_get_subject_name(сертификат));
    ok = 1;
выход:
    EVP_PKEY_free(ключ);
    X509_free(сертификат);
    sk_X509_pop_free(цепочка, X509_free);
    return ok;
}

static int команда_tls(const char *узел, const char *порт, const char *корень,
                       const char *клиент_p12)
{
    SSL_CTX *контекст = NULL;
    BIO *соединение = NULL;
    SSL *ssl = NULL;
    X509 *сервер = NULL;
    int итог = 2;
    контекст = SSL_CTX_new(TLS_client_method());
    if (!контекст) goto openssl_ошибка;
    if (SSL_CTX_set_min_proto_version(контекст, TLS1_2_VERSION) != 1) goto openssl_ошибка;
    if (SSL_CTX_set_cipher_list(контекст,
        "HIGH:!aNULL:!eNULL:!MD5:!RC4:!3DES:!DES:!EXPORT:!PSK:!SRP") != 1 ||
        SSL_CTX_set_ciphersuites(контекст,
        "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256") != 1)
        goto openssl_ошибка;
    SSL_CTX_set_verify(контекст, SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_set_default_verify_paths(контекст) != 1) {
        fprintf(stderr, "Предупреждение: системное хранилище OpenSSL недоступно.\n");
        ERR_clear_error();
    }
    if (корень && SSL_CTX_load_verify_locations(контекст, корень, NULL) != 1) {
        показать_ошибку_openssl("не удалось загрузить явно указанный CA-файл");
        goto выход;
    }
    if (клиент_p12 && !подключить_p12(контекст, клиент_p12)) goto выход;
    соединение = BIO_new_ssl_connect(контекст);
    if (!соединение) goto openssl_ошибка;
    if (BIO_set_conn_hostname(соединение, узел) != 1 ||
        BIO_set_conn_port(соединение, порт) != 1) {
        fprintf(stderr, "Не удалось настроить адрес TLS-сервера.\n");
        goto выход;
    }
    if (BIO_get_ssl(соединение, &ssl) <= 0 || !ssl) goto openssl_ошибка;
    if (SSL_set_tlsext_host_name(ssl, узел) != 1 || SSL_set1_host(ssl, узел) != 1)
        goto openssl_ошибка;
    printf("Подключение к %s:%s; проверка цепочки и имени узла включена.\n", узел, порт);
    if (BIO_do_connect(соединение) <= 0 || BIO_do_handshake(соединение) <= 0) {
        показать_ошибку_openssl("TLS-соединение не установлено или сертификат не прошёл проверку");
        if (ssl) {
            long vr = SSL_get_verify_result(ssl);
            fprintf(stderr, "Проверка сертификата: %s\n", X509_verify_cert_error_string(vr));
        }
        goto выход;
    }
    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        fprintf(stderr, "Сертификат сервера не прошёл проверку: %s\n",
                X509_verify_cert_error_string(SSL_get_verify_result(ssl)));
        goto выход;
    }
    сервер = SSL_get1_peer_certificate(ssl);
    printf("TLS установлен: %s\n", SSL_get_version(ssl));
    printf("Набор шифров: %s\n", SSL_get_cipher_name(ssl));
    printf("Проверка имени узла и цепочки: успешно.\n");
    if (сервер) {
        (void)напечатать_имя("Сервер", X509_get_subject_name(сервер));
        напечатать_дату("Сертификат действует до", X509_get0_notAfter(сервер));
        (void)напечатать_отпечаток(сервер);
    }
    итог = 0;
    goto выход;
openssl_ошибка:
    показать_ошибку_openssl("не удалось создать или настроить TLS-клиент");
выход:
    X509_free(сервер);
    BIO_free_all(соединение);
    SSL_CTX_free(контекст);
    return итог;
}

static void справка(void)
{
    puts("Лексус — корпоративная диагностика TLS и сертификатов");
    puts("Команды:");
    puts("  лексус-сертификаты инфо <сертификат.cer|crt|pem>");
    puts("  лексус-сертификаты проверить <сертификат> <корень> [промежуточный]");
    puts("  лексус-сертификаты p12 <клиент.p12>");
    puts("  лексус-сертификаты tls <домен> [порт] [--корень файл] [--клиент-p12 файл]");
    puts("  лексус-сертификаты профили");
    puts("");
    puts("Пароль PKCS#12 вводится скрыто в интерактивном терминале.");
    puts("Закрытый ключ не выводится и не сохраняется в отдельный файл.");
}

static void профили(void)
{
    puts("Профили сертификатов");
    puts("");
    puts("Минцифры / российские HTTPS-сайты:");
    puts("  Сертификаты Russian Trusted Root CA и Russian Trusted Sub CA");
    puts("  скачивайте только с https://www.gosuslugi.ru/crt");
    puts("  Для RSA-варианта используйте явный CA-файл через --корень.");
    puts("  Промежуточный сертификат передавайте команде «проверить», если он нужен.");
    puts("  Диагностика не устанавливает сертификаты в системное хранилище.");
    puts("");
    puts("Сбер API:");
    puts("  API использует mTLS. Клиентский контейнер PKCS#12 (.p12) можно");
    puts("  проверить командой «p12» и подключить к TLS-проверке через --клиент-p12.");
    puts("  Сверяйте clientId и срок действия с кабинетом Sber API.");
    puts("");
    puts("ГОСТ TLS / электронная подпись:");
    puts("  ГОСТ — отдельный криптографический профиль, а не просто CA-файл.");
    puts("  Нужен совместимый сертифицированный криптопровайдер (например,");
    puts("  КриптоПро CSP) и интеграция TLS на поддерживаемой платформе.");
    puts("  Этот инструмент не реализует ГОСТ самостоятельно и не заявляет,");
    puts("  что OpenSSL-сборка заменяет КриптоПро CSP.");
    puts("");
    puts("OpenSSL-сборка проверяет поддерживаемые ею алгоритмы и сертификаты.");
    puts("Для ГОСТ-вариантов цепочки используйте штатный ГОСТ-провайдер.");
}

int main(int argc, char **argv)
{
    if (OPENSSL_VERSION_MAJOR < 3) {
        fprintf(stderr, "Для инструмента требуется OpenSSL 3.0 или новее.\n");
        return 2;
    }
    if (argc < 2 || strcmp(argv[1], "помощь") == 0 || strcmp(argv[1], "--help") == 0) {
        справка();
        return argc < 2 ? 0 : 0;
    }
    if (strcmp(argv[1], "профили") == 0) {
        профили();
        return 0;
    }
    if (strcmp(argv[1], "инфо") == 0 && argc == 3)
        return команда_инфо(argv[2]);
    if (strcmp(argv[1], "проверить") == 0 && (argc == 4 || argc == 5))
        return команда_проверить(argv[2], argv[3], argc == 5 ? argv[4] : NULL);
    if (strcmp(argv[1], "p12") == 0 && argc == 3)
        return команда_p12(argv[2]);
    if (strcmp(argv[1], "tls") == 0 && argc >= 3) {
        const char *порт = "443", *корень = NULL, *клиент = NULL;
        int i = 3;
        if (i < argc && argv[i][0] != '-') порт = argv[i++];
        while (i < argc) {
            if (strcmp(argv[i], "--корень") == 0 && i + 1 < argc) {
                корень = argv[i + 1]; i += 2;
            } else if (strcmp(argv[i], "--клиент-p12") == 0 && i + 1 < argc) {
                клиент = argv[i + 1]; i += 2;
            } else {
                fprintf(stderr, "Неизвестный параметр TLS: %s\n", argv[i]);
                return 2;
            }
        }
        return команда_tls(argv[2], порт, корень, клиент);
    }
    справка();
    return 2;
}
