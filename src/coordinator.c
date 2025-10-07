/* coordinator.c - Coordenador para Quebra de Senhas Paralelo
 *
 * Uso: ./coordinator <hash_md5> <tamanho> <charset> <num_workers>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#define MAX_WORKERS 16
#define RESULT_FILE "password_found.txt"

/* calcula o número total de combinações */
long long calculate_search_space(int charset_len, int password_len) {
    long long total = 1;
    for (int i = 0; i < password_len; i++) total *= charset_len;
    return total;
}

/* converte índice -> senha (lexicográfico) */
void index_to_password(long long index, const char *charset, int charset_len,
                       int password_len, char *output) {
    for (int i = password_len - 1; i >= 0; i--) {
        output[i] = charset[index % charset_len];
        index /= charset_len;
    }
    output[password_len] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <hash_md5> <tamanho> <charset> <num_workers>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s 900150983cd24fb0d6963f7d28e17f72 3 abc 4\n", argv[0]);
        return 1;
    }

    const char *target_hash = argv[1];
    int password_len = atoi(argv[2]);
    const char *charset = argv[3];
    int num_workers = atoi(argv[4]);
    int charset_len = (int)strlen(charset);

    if (password_len <= 0 || password_len > 10) {
        fprintf(stderr, "Erro: O tamanho da senha deve estar entre 1 e 10.\n");
        return 1;
    }
    if (num_workers <= 0 || num_workers > MAX_WORKERS) {
        fprintf(stderr, "Erro: O número de workers deve estar entre 1 e %d.\n", MAX_WORKERS);
        return 1;
    }
    if (charset_len == 0) {
        fprintf(stderr, "Erro: O charset não pode ser vazio.\n");
        return 1;
    }

    printf("=== Mini-Projeto 1: Quebra de Senhas Paralelo ===\n");
    printf("Hash MD5 alvo: %s\n", target_hash);
    printf("Tamanho da senha: %d\n", password_len);
    printf("Charset: %s (tamanho: %d)\n", charset, charset_len);
    printf("Número de workers: %d\n", num_workers);

    long long total_space = calculate_search_space(charset_len, password_len);
    printf("Espaço de busca total: %lld combinações\n\n", total_space);

    /* remover resultado anterior */
    unlink(RESULT_FILE);

    time_t start_time = time(NULL);

    /* divisão */
    long long passwords_per_worker = total_space / num_workers;
    long long remaining = total_space % num_workers;

    pid_t workers[MAX_WORKERS];
    for (int i = 0; i < num_workers; ++i) workers[i] = -1;

    long long current_start_index = 0;
    printf("Iniciando workers...\n");

    for (int i = 0; i < num_workers; ++i) {
        long long range_size = passwords_per_worker + (i < remaining ? 1 : 0);

        if (range_size == 0) {
            /* nada para esse worker */
            printf("Worker %d: sem intervalo (range_size == 0), pulando.\n", i);
            continue;
        }

        long long end_index = current_start_index + range_size; /* exclusive */

        /* buffers para senhas (inclui terminador) */
        char start_pass[password_len + 1];
        char end_pass[password_len + 1];

        index_to_password(current_start_index, charset, charset_len, password_len, start_pass);
        index_to_password(end_index - 1, charset, charset_len, password_len, end_pass);

        /* preparar strings para execl */
        char worker_id_str[16];
        char password_len_str[16];
        snprintf(worker_id_str, sizeof(worker_id_str), "%d", i);
        snprintf(password_len_str, sizeof(password_len_str), "%d", password_len);

        pid_t pid = fork();
        if (pid < 0) {
            perror("Erro no fork()");
            /* atualizar current_start_index e continuar tentativa com menos workers? aqui abortamos */
            exit(1);
        } else if (pid == 0) {
            /* filho -> executar worker */
            /* ordem de args exigida pelo worker: <hash> <start> <end> <charset> <len> <id> */
            execl("./worker", "worker",
                  target_hash,
                  start_pass,
                  end_pass,
                  charset,
                  password_len_str,
                  worker_id_str,
                  (char *)NULL);
            /* se execl retornar, houve erro */
            perror("Erro no execl()");
            _exit(1);
        } else {
            /* pai */
            workers[i] = pid;
            printf("Worker %d criado: PID %d  intervalo: '%s' -> '%s'\n", i, pid, start_pass, end_pass);
        }

        current_start_index = end_index;
    }

    printf("\nTodos os workers foram iniciados. Aguardando conclusão...\n");

    /* aguardar filhos (somente os que foram criados) */
    int workers_finished = 0;
    while (workers_finished < num_workers) {
        int status;
        pid_t terminated_pid = wait(&status);
        if (terminated_pid <= 0) {
            /* wait pode falhar se não houver filhos */
            break;
        }
        /* localizar qual worker foi */
        int worker_id = -1;
        for (int i = 0; i < num_workers; ++i) {
            if (workers[i] == terminated_pid) {
                worker_id = i;
                workers[i] = -1; /* já tratado */
                break;
            }
        }

        if (worker_id == -1) {
            /* PID não reconhecido (pouco provável) */
            printf("Filho (PID %d) terminou (não mapeado a worker).\n", terminated_pid);
        } else {
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (exit_code == 0) {
                    printf("Worker %d (PID %d) terminou: status 0.\n", worker_id, terminated_pid);
                } else {
                    printf("Worker %d (PID %d) terminou: status %d.\n", worker_id, terminated_pid, exit_code);
                }
            } else {
                printf("Worker %d (PID %d) terminou de forma anormal.\n", worker_id, terminated_pid);
            }
            workers_finished++;
        }
    }

    time_t end_time = time(NULL);
    double elapsed_time = difftime(end_time, start_time);

    printf("\n=== Resultado ===\n");

    /* verificar se resultado existe e imprimir senha */
    FILE *result_file = fopen(RESULT_FILE, "r");
    if (result_file != NULL) {
        char line[256];
        if (fgets(line, sizeof(line), result_file)) {
            /* formato esperado: worker_id:password\n */
            line[strcspn(line, "\n")] = '\0';
            char *p = strchr(line, ':');
            if (p) {
                printf("SENHA ENCONTRADA: %s\n", p + 1);
            } else {
                printf("Arquivo de resultado encontrado, conteúdo: %s\n", line);
            }
        }
        fclose(result_file);
    } else {
        printf("Senha não encontrada no espaço de busca.\n");
    }

    printf("Tempo total de execução: %.2f segundos\n", elapsed_time);
    if (elapsed_time > 0.0) {
        printf("Performance (aprox): %.2f hashes/segundo\n", (double)total_space / elapsed_time);
    }

    return 0;
}
