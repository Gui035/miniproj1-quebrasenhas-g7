#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "hash_utils.h"

#define RESULT_FILE "password_found.txt"
#define PROGRESS_INTERVAL 1000000
#define MD5_STRING_SIZE 33


/* incrementa senha no charset (como contador) */
void increment_password(char *password, const char *charset, int charset_len, int password_len) {
    for (int i = password_len - 1; i >= 0; i--) {
        char *pos = strchr(charset, password[i]);
        int idx = pos - charset;
        if (idx < charset_len - 1) {
            password[i] = charset[idx + 1];
            return;
        } else {
            password[i] = charset[0]; // carry
        }
    }
}

/* salva senha encontrada em arquivo (atomicamente) */
int save_result(int worker_id, const char *password) {
    int fd = open(RESULT_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1) {
        // já existe → outro worker encontrou primeiro
        return 0;
    }
    dprintf(fd, "%d:%s\n", worker_id, password);
    close(fd);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Uso: %s <hash> <start> <end> <charset> <len> <worker_id>\n", argv[0]);
        return 1;
    }

    const char *target_hash = argv[1];
    char current[128];
    char end[128];
    strncpy(current, argv[2], sizeof(current));
    strncpy(end, argv[3], sizeof(end));
    const char *charset = argv[4];
    int password_len = atoi(argv[5]);
    int worker_id = atoi(argv[6]);
    int charset_len = strlen(charset);

    char hash[MD5_STRING_SIZE];
    long long tested = 0;

    while (1) {
        md5_string(current, hash);

        if (strcmp(hash, target_hash) == 0) {
            if (save_result(worker_id, current)) {
                printf("Worker %d encontrou a senha: %s\n", worker_id, current);
            }
            return 0;
        }

        if (strcmp(current, end) == 0) break; // chegou no fim

        increment_password(current, charset, charset_len, password_len);

        if (++tested % PROGRESS_INTERVAL == 0) {
            if (access(RESULT_FILE, F_OK) == 0) return 0; // outro já achou
        }
    }

    return 0;
}
