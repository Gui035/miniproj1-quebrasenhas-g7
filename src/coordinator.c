#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include "hash_utils.h"

/**
 * PROCESSO COORDENADOR - Mini-Projeto 1: Quebra de Senhas Paralelo
 * * Este programa coordena múltiplos workers para quebrar senhas MD5 em paralelo.
 * O MD5 JÁ ESTÁ IMPLEMENTADO - você deve focar na paralelização (fork/exec/wait).
 * * Uso: ./coordinator <hash_md5> <tamanho> <charset> <num_workers>
 * * Exemplo: ./coordinator "900150983cd24fb0d6963f7d28e17f72" 3 "abc" 4
 * * SEU TRABALHO: Implementar os TODOs marcados abaixo
 */

#define MAX_WORKERS 16
#define RESULT_FILE "password_found.txt"

/**
 * Calcula o tamanho total do espaço de busca
 * * @param charset_len Tamanho do conjunto de caracteres
 * @param password_len Comprimento da senha
 * @return Número total de combinações possíveis
 */
long long calculate_search_space(int charset_len, int password_len) {
    long long total = 1;
    for (int i = 0; i < password_len; i++) {
        total *= charset_len;
    }
    return total;
}

/**
 * Converte um índice numérico para uma senha
 * Usado para definir os limites de cada worker
 * * @param index Índice numérico da senha
 * @param charset Conjunto de caracteres
 * @param charset_len Tamanho do conjunto
 * @param password_len Comprimento da senha
 * @param output Buffer para armazenar a senha gerada
 */
void index_to_password(long long index, const char *charset, int charset_len, 
                       int password_len, char *output) {
    for (int i = password_len - 1; i >= 0; i--) {
        output[i] = charset[index % charset_len];
        index /= charset_len;
    }
    output[password_len] = '\0';
}

/**
 * Função principal do coordenador
 */
int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <hash_md5> <tamanho> <charset> <num_workers>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s 900150983cd24fb0d6963f7d28e17f72 3 abc 4\n", argv[0]);
        exit(1);
    }
    // TODO 1: Validar argumentos de entrada
    // Verificar se argc == 5 (programa + 4 argumentos)
    // Se não, imprimir mensagem de uso e sair com código 1
    
    // IMPLEMENTE AQUI: verificação de argc e mensagem de erro
    
    // Parsing dos argumentos (após validação)
    const char *target_hash = argv[1];
    int password_len = atoi(argv[2]);
    const char *charset = argv[3];
    int num_workers = atoi(argv[4]);
    int charset_len = strlen(charset);

    if (password_len <= 0 || password_len > 10) {
        fprintf(stderr, "Erro: O tamanho da senha deve estar entre 1 e 10.\n");
        exit(1);
    }
    
    if (num_workers <= 0 || num_workers > MAX_WORKERS) {
        fprintf(stderr, "Erro: O número de workers deve estar entre 1 e %d.\n", MAX_WORKERS);
        exit(1);
    }

    if (charset_len == 0) {
        fprintf(stderr, "Erro: O charset não pode ser vazio.\n");
        exit(1);
    }
    
    // TODO: Adicionar validações dos parâmetros
    // - password_len deve estar entre 1 e 10
    // - num_workers deve estar entre 1 e MAX_WORKERS
    // - charset não pode ser vazio
    
    printf("=== Mini-Projeto 1: Quebra de Senhas Paralelo ===\n");
    printf("Hash MD5 alvo: %s\n", target_hash);
    printf("Tamanho da senha: %d\n", password_len);
    printf("Charset: %s (tamanho: %d)\n", charset, charset_len);
    printf("Número de workers: %d\n", num_workers);
    
    // Calcular espaço de busca total
    long long total_space = calculate_search_space(charset_len, password_len);
    printf("Espaço de busca total: %lld combinações\n\n", total_space);
    
    // Remover arquivo de resultado anterior se existir
    unlink(RESULT_FILE);
    
    // Registrar tempo de início
    time_t start_time = time(NULL);
    
    // TODO 2: Dividir o espaço de busca entre os workers
    // Calcular quantas senhas cada worker deve verificar
    // DICA: Use divisão inteira e distribua o resto entre os primeiros workers
    
    // IMPLEMENTE AQUI:
    // long long passwords_per_worker = ?
    // long long remaining = ?
    long long passwords_per_worker = total_space / num_workers;
    long long remaining = total_space % num_workers;
    
    // Arrays para armazenar PIDs dos workers
    pid_t workers[MAX_WORKERS];
    
    // TODO 3: Criar os processos workers usando fork()
    long long current_start_index = 0;
    printf("Iniciando workers...\n");
    
    // IMPLEMENTE AQUI: Loop para criar workers
    for (int i = 0; i < num_workers; i++) {
        long long range_size = passwords_per_worker + (i < remaining ? 1 : 0);
        long long end_index = current_start_index + range_size;
        
        char start_pass[password_len + 1];
        char end_pass[password_len + 1];
        
        index_to_password(current_start_index, charset, charset_len, password_len, start_pass);
        index_to_password(end_index - 1, charset, charset_len, password_len, end_pass);

        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Erro no fork()");
            exit(1);
        } else if (pid == 0) {
            char worker_id_str[10];
            sprintf(worker_id_str, "%d", i);
            printf("Worker %d: PID %d, buscando de '%s' até '%s'\n", 
                   i, getpid(), start_pass, end_pass);
            execl("./worker", "worker", target_hash, charset, start_pass, end_pass, worker_id_str, (char *)NULL);
            perror("Erro no execl()");
            exit(1);
        } else {
            workers[i] = pid;
        }

        current_start_index = end_index;
    }
    
    printf("\nTodos os workers foram iniciados. Aguardando conclusão...\n");
    
    // TODO 8: Aguardar todos os workers terminarem usando wait()
    // IMPORTANTE: O pai deve aguardar TODOS os filhos para evitar zumbis
    
    // IMPLEMENTE AQUI:
    int workers_finished = 0;
    while (workers_finished < num_workers) {
        int status;
        pid_t terminated_pid = wait(&status);

        if (terminated_pid > 0) {
            int worker_id = -1;
            for (int i = 0; i < num_workers; i++) {
                if (workers[i] == terminated_pid) {
                    worker_id = i;
                    break;
                }
            }

            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (exit_code == 0) {
                    printf("Worker %d (PID %d) terminou: senha encontrada!\n", worker_id, terminated_pid);
                } else if (exit_code == 1) {
                    printf("Worker %d (PID %d) terminou: senha não encontrada em seu intervalo.\n", worker_id, terminated_pid);
                }
            } else {
                printf("Worker %d (PID %d) terminou de forma anormal.\n", worker_id, terminated_pid);
            }
            workers_finished++;
        }
    }
    // - Loop para aguardar cada worker terminar
    // - Usar wait() para capturar status de saída
    // - Identificar qual worker terminou
    // - Verificar se terminou normalmente ou com erro
    // - Contar quantos workers terminaram
    
    // Registrar tempo de fim
    time_t end_time = time(NULL);
    double elapsed_time = difftime(end_time, start_time);
    
    printf("\n=== Resultado ===\n");
    
    // TODO 9: Verificar se algum worker encontrou a senha
    // Ler o arquivo password_found.txt se existir
    
    // IMPLEMENTE AQUI:
    FILE *result_file = fopen(RESULT_FILE, "r");
    
    if (result_file != NULL) {
        char line_buffer[password_len + 10]; 

        if (fgets(line_buffer, sizeof(line_buffer), result_file)) {
            line_buffer[strcspn(line_buffer, "\n")] = 0;

            char* password_only = strchr(line_buffer, ':');
            if (password_only) {
                password_only++; 
                printf("SENHA ENCONTRADA: %s\n", password_only);
            }
        }
        fclose(result_file);
    } else {
        printf("Senha não encontrada no espaço de busca.\n");
    }
    
    printf("Tempo total de execução: %.2f segundos\n", elapsed_time);
    if (elapsed_time > 0) {
        printf("Performance: %.2f hashes/segundo\n", (double)total_space / elapsed_time);
    }
    // - Abrir arquivo RESULT_FILE para leitura
    // - Ler conteúdo do arquivo
    // - Fazer parse do formato "worker_id:password"
    // - Verificar o hash usando md5_string()
    // - Exibir resultado encontrado
    
    // Estatísticas finais (opcional)
    // TODO: Calcular e exibir estatísticas de performance
    
    return 0;
}
