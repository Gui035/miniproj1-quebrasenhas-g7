# Relatório: Mini-Projeto 1 - Quebra-Senhas Paralelo

**Aluno(s):** Guillermo Martinez 10418697, Felipe Marques 10437877
---

## 1. Estratégia de Paralelização


**Como você dividiu o espaço de busca entre os workers?**

Dividi o espaço de busca de forma uniforme entre os workers.
A ideia foi converter o espaço de busca total em números (índices) e atribuir a cada worker um intervalo contínuo.

**Código relevante:** Cole aqui a parte do coordinator.c onde você calcula a divisão:
```c
/* calcula o número total de combinações */
long long calculate_search_space(int charset_len, int password_len) {
    long long total = 1;
    for (int i = 0; i < password_len; i++) total *= charset_len;
    return total;
}

/* conversão índice -> senha (lexicográfico) */
void index_to_password(long long index, const char *charset, int charset_len,
                       int password_len, char *output) {
    for (int i = password_len - 1; i >= 0; i--) {
        output[i] = charset[index % charset_len];
        index /= charset_len;
    }
    output[password_len] = '\0';
}

/* ...no main(), após calcular total_space... */

/* divisão */
long long passwords_per_worker = total_space / num_workers;
long long remaining = total_space % num_workers;

long long current_start_index = 0;
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

    /* ...aqui vem fork/exec (veja abaixo) ... */

    current_start_index = end_index;
}


```

---

## 2. Implementação das System Calls

**Descreva como você usou fork(), execl() e wait() no coordinator:**

Usei um loop de fork para criar num_workers processos filhos.
Cada filho executa o binário worker através de execl(), recebendo como argumentos: o hash alvo, senha inicial, senha final, charset, tamanho da senha e o ID do worker.
O processo pai continua criando os demais workers, e ao final usa wait() para aguardar a conclusão de todos, garantindo que nenhum processo zumbi permaneça., recebendo como argumentos: o hash alvo, senha inicial, senha final, charset, tamanho da senha e o ID do worker.

**Código do fork/exec:**
```c
/* preparar strings para execl */
char worker_id_str[16];
char password_len_str[16];
snprintf(worker_id_str, sizeof(worker_id_str), "%d", i);
snprintf(password_len_str, sizeof(password_len_str), "%d", password_len);

pid_t pid = fork();
if (pid < 0) {
    perror("Erro no fork()");
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

```

---

## 3. Comunicação Entre Processos

**Como você garantiu que apenas um worker escrevesse o resultado?**
Para garantir que apenas um worker escrevesse o resultado, utilizei uma operação atômica fornecida pelo próprio sistema operacional através da chamada de sistema open(). O caso é que o sistema operacional garante que a verificação da existência do arquivo e sua criação são uma única operação atômica (indivisível). Não há como outro processo interferir entre o momento em que o SO verifica que o arquivo não existe e o momento em que ele o cria.

**Como o coordinator consegue ler o resultado?**
O coordinator lê e analisa (faz o parse) do arquivo de resultado na etapa final de sua execução, após ter esperado por todos os processos worker. O coordinator primeiro tenta abrir o arquivo RESULT_FILE, uma vez que o arquivo está aberto, o programa lê a única linha de conteúdo que ele deve ter. Depois de ler a linha, o programa a manipula para extrair apenas a senha.

---

## 4. Análise de Performance
Complete a tabela com tempos reais de execução:
O speedup é o tempo do teste com 1 worker dividido pelo tempo com 4 workers.

| Teste | 1 Worker | 2 Workers | 4 Workers | Speedup (4w) |
|-------|----------|-----------|-----------|--------------|
| Hash: 202cb962ac59075b964b07152d234b70<br>Charset: "0123456789"<br>Tamanho: 3<br>Senha: "123" | 0.01s | 0.01s | 0.01s | 1.0x |
| Hash: 5d41402abc4b2a76b9719d911017c592<br>Charset: "abcdefghijklmnopqrstuvwxyz"<br>Tamanho: 5<br>Senha: "hello" | 4.52s | 2.27s | 1.14s | 3.96x |

**O speedup foi linear? Por quê?**
Não, o speedup não foi linear para o teste 1 (senha "123"). O speedup foi de aproximadamente 1.0x, o que significa que não houve ganho de velocidade. Isso acontece porque a tarefa era extremamente curta. O tempo total de execução foi dominado pela sobrecarga (overhead) do sistema operacional para criar e gerenciar os processos, e não pelo trabalho de quebrar a senha em si. Entretanto, para o teste 2 (senha "hello") o speedup foi quase perfeitamente linear. O speedup de 3.96x com 4 workers é muito próximo do speedup ideal de 4x. Isso acontece por duas razões principais que tornam este problema ideal para paralelização:CPU-bound: o trabalho dos workers consiste quase inteiramente em fazer cálculos (gerar hashes MD5). Eles não perdem tempo esperando por operações lentas como leitura de disco ou rede (I/O). Além de que trata-se de uma tarefa altamente paralizável. 

---

## 5. Desafios e Aprendizados
**Qual foi o maior desafio técnico que você enfrentou?**
O maior problema enfrentado foi no desenvolvimento do coordinator, o processo filho é uma cópia exata do processo pai e a grande dificuldade foi no momento do código que o filho chama o execl(), tendo em vista que ele apaga a memória do processo filho para que o worker possa funcionar, porém, por ter apagado a memória, o worker perde o acesso às variáveis (start_pass, end_pass), por exemplo, e por isso a grande dificuldade foi passar as informações únicas a cada iteração do loop. 

---

## Comandos de Teste Utilizados

```bash
# Teste básico
./coordinator "900150983cd24fb0d6963f7d28e17f72" 3 "abc" 2

# Teste de performance
time ./coordinator "202cb962ac59075b964b07152d234b70" 3 "0123456789" 1
time ./coordinator "202cb962ac59075b964b07152d234b70" 3 "0123456789" 4

# Teste com senha maior
time ./coordinator "5d41402abc4b2a76b9719d911017c592" 5 "abcdefghijklmnopqrstuvwxyz" 4
```
---

**Checklist de Entrega:**
- [ ] Código compila sem erros
- [ ] Todos os TODOs foram implementados
- [ ] Testes passam no `./tests/simple_test.sh`
- [ ] Relatório preenchido
