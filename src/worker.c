/* worker.c - Processo TRABALHADOR para Quebra de Senhas Paralelo
 *
 * Uso: ./worker <hash_alvo> <senha_inicial> <senha_final> <charset> <len> <id>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include "hash_utils.h"

#define RESULT_FILE "password_found.txt"
/* Checar com frequência*/
#define PROGRESS_INTERVAL 1024

/* Constrói um mapa de índices para charset: map[(unsigned char)c] = index in charset or -1 */
static void build_index_map(const char *charset, int charset_len, int map_out[256]) {
    for (int i = 0; i < 256; ++i) map_out[i] = -1;
    for (int i = 0; i < charset_len; ++i) {
        unsigned char c = (unsigned char) charset[i];
        map_out[c] = i;
    }
}

/* Left-pad src into dst so that dst has length password_len.
 * Pad char is pad_char. dst must have space for password_len+1.
 */
static void pad_left_password(char *dst, const char *src, int password_len, char pad_char) {
    int src_len = (int)strlen(src);
    if (src_len >= password_len) {
        /* If longer or equal, copy only the leftmost password_len chars */
        strncpy(dst, src, password_len);
        dst[password_len] = '\0';
        return;
    }
    int pad = password_len - src_len;
    for (int i = 0; i < pad; ++i) dst[i] = pad_char;
    memcpy(dst + pad, src, src_len);
    dst[password_len] = '\0';
}

/* Normaliza string hex (32 chars) para lowercase */
static void strtolower_inplace(char *s) {
    while (*s) {
        *s = (char) tolower((unsigned char)*s);
        s++;
    }
}

/**
 * Incrementa uma senha para a próxima na ordem lexicográfica usando mapa de índices
 * @return 1 se incrementou com sucesso, 0 se overflow (acabou o espaço), -1 se caractere inválido
 */
static int increment_password_map(char *password, const int map[256], const char *charset, int charset_len, int password_len) {
    for (int pos = password_len - 1; pos >= 0; --pos) {
        unsigned char cur = (unsigned char) password[pos];
        int idx = map[cur];
        if (idx < 0) return -1; /* caractere inválido */
        if (idx + 1 < charset_len) {
            password[pos] = charset[idx + 1];
            return 1; /* incremento bem sucedido */
        } else {
            /* overflow nesta posição: volta ao primeiro caractere e propaga */
            password[pos] = charset[0];
        }
    }
    return 0; /* estourou todas as posições (acabou o espaço) */
}

/* Compara lexicograficamente (strings C normais) */
static int password_compare(const char *a, const char *b) {
    return strcmp(a, b);
}

/* Verifica se arquivo resultado existe */
static int check_result_exists() {
    return access(RESULT_FILE, F_OK) == 0;
}

/* Grava resultado de forma atômica (O_CREAT | O_EXCL). Faz fsync para garantir escrita em disco. */
static void save_result(int worker_id, const char *password) {
    int fd = open(RESULT_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) {
        if (errno == EEXIST) {
            /* Outro worker ganhou a corrida */
            fprintf(stderr, "[Worker %d] save_result: arquivo já existe (outro worker escreveu).\n", worker_id);
            return;
        } else {
            fprintf(stderr, "[Worker %d] save_result: open() falhou: %s\n", worker_id, strerror(errno));
            return;
        }
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf), "%d:%s\n", worker_id, password);
    ssize_t w = write(fd, buf, (size_t)len);
    if (w < 0 || w < len) {
        fprintf(stderr, "[Worker %d] save_result: write() incompleto/erro: %s\n", worker_id, strerror(errno));
        /* Ainda fechamos e retornamos */
    } else {
        /* garantir flush */
        if (fsync(fd) != 0) {
            /* não fatal, só avisar */
            fprintf(stderr, "[Worker %d] save_result: fsync() falhou: %s\n", worker_id, strerror(errno));
        } else {
            fprintf(stderr, "[Worker %d] Senha gravada com sucesso: %s\n", worker_id, password);
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Uso interno: %s <hash> <start> <end> <charset> <len> <id>\n", argv[0]);
        return 1;
    }

    const char *target_hash_arg = argv[1];
    const char *start_arg = argv[2];
    const char *end_arg = argv[3];
    const char *charset = argv[4];
    int password_len = atoi(argv[5]);
    int worker_id = atoi(argv[6]);

    if (password_len <= 0) {
        fprintf(stderr, "[Worker %d] password_len inválido: %d\n", worker_id, password_len);
        return 1;
    }

    int charset_len = (int)strlen(charset);
    if (charset_len <= 0) {
        fprintf(stderr, "[Worker %d] charset vazio\n", worker_id);
        return 1;
    }

    /* Buffers para senhas (tamanho seguro) */
    char start_password[128], end_password[128], current_password[128];
    if (password_len >= (int)sizeof(start_password)) {
        fprintf(stderr, "[Worker %d] password_len muito grande\n", worker_id);
        return 1;
    }

    /* Pad / trim para garantir comprimento correto */
    pad_left_password(start_password, start_arg, password_len, charset[0]);
    pad_left_password(end_password, end_arg, password_len, charset[0]);

    /* Build index map */
    int idx_map[256];
    build_index_map(charset, charset_len, idx_map);

    /* Validate that start and end contain only charset chars */
    for (int i = 0; i < password_len; ++i) {
        if (idx_map[(unsigned char)start_password[i]] < 0) {
            fprintf(stderr, "[Worker %d] start_password contém caractere não presente no charset: '%c'\n", worker_id, start_password[i]);
            return 1;
        }
        if (idx_map[(unsigned char)end_password[i]] < 0) {
            fprintf(stderr, "[Worker %d] end_password contém caractere não presente no charset: '%c'\n", worker_id, end_password[i]);
            return 1;
        }
    }

    /* Prepare hashes */
    char target_hash[128];
    strncpy(target_hash, target_hash_arg ? target_hash_arg : "", sizeof(target_hash)-1);
    target_hash[sizeof(target_hash)-1] = '\0';
    strtolower_inplace(target_hash);

    /* Inicial current = start */
    strncpy(current_password, start_password, sizeof(current_password)-1);
    current_password[sizeof(current_password)-1] = '\0';

    /* Debug inicial (stderr para garantir visibilidade) */
    fprintf(stderr, "[Worker %d] PID %d iniciado. intervalo: %s -> %s  charset_len=%d password_len=%d\n",
            worker_id, (int)getpid(), start_password, end_password, charset_len, password_len);

    char computed_hash[64];
    long long passwords_checked = 0;
    time_t t_start = time(NULL);

    /* Loop principal */
    while (1) {
        /* Checagem frequentemente para pequenos espaços; evita continuar após outro worker ter ganho */
        if ((passwords_checked % PROGRESS_INTERVAL) == 0) {
            if (check_result_exists()) {
                fprintf(stderr, "[Worker %d] Encerrando: resultado já existe.\n", worker_id);
                break;
            }
        }

        /* Calcula MD5 */
        md5_string(current_password, computed_hash);
        computed_hash[32] = '\0'; /* garantir terminação */
        strtolower_inplace(computed_hash);

        if (strcmp(computed_hash, target_hash) == 0) {
            /* Encontrou! tenta gravar e termina */
            fprintf(stderr, "[Worker %d] Encontrou senha: %s (hash=%s)\n", worker_id, current_password, computed_hash);
            save_result(worker_id, current_password);
            break;
        }

        /* Se passou do fim do intervalo, terminamos */
        if (password_compare(current_password, end_password) >= 0) {
            /* chegamos ou ultrapassamos o end */
            break;
        }

        /* Incrementa */
        int inc = increment_password_map(current_password, idx_map, charset, charset_len, password_len);
        if (inc == 1) {
            /* sucesso */
        } else if (inc == 0) {
            /* estourou todo o espaço */
            break;
        } else { /* inc == -1 */
            fprintf(stderr, "[Worker %d] Erro: caractere inválido encontrado ao incrementar (senha='%s')\n", worker_id, current_password);
            break;
        }

        passwords_checked++;
    }

    time_t t_end = time(NULL);
    double total_time = difftime(t_end, t_start);
    fprintf(stderr, "[Worker %d] Finalizado. Checadas: %lld senhas em %.2f s\n", worker_id, passwords_checked, total_time);
    if (total_time > 0.0) {
        fprintf(stderr, "[Worker %d] Taxa aproximada: %.0f senhas/s\n", worker_id, passwords_checked / total_time);
    }

    return 0;
}
