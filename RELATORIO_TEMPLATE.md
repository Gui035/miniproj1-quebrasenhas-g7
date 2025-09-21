# Relatório: Mini-Projeto 1 - Quebra-Senhas Paralelo

**Aluno(s):** Guillermo Martinez 10418697, Nome (Matrícula),,,  
---

## 1. Estratégia de Paralelização


**Como você dividiu o espaço de busca entre os workers?**

Dividi o espaço de busca de forma uniforme entre os workers.
A ideia foi converter o espaço de busca total em números (índices) e atribuir a cada worker um intervalo contínuo.

**Código relevante:** Cole aqui a parte do coordinator.c onde você calcula a divisão:
```c
long long total = 1;
for (int i = 0; i < password_len; i++) {
    total *= charset_len;
}

long long chunk = total / num_workers;
long long remainder = total % num_workers;

for (int i = 0; i < num_workers; i++) {
    long long start_index = i * chunk + (i < remainder ? i : remainder);
    long long end_index   = start_index + chunk - 1;
    if (i < remainder) end_index++;

    char start_pass[128], end_pass[128];
    index_to_password(start_index, charset, password_len, start_pass);
    index_to_password(end_index,   charset, password_len, end_pass);

    // execl chamando worker com start_pass e end_pass
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
for (int i = 0; i < num_workers; i++) {
    pid_t pid = fork();
    if (pid == 0) {
        // processo filho
        execl("./worker", "./worker",
              target_hash, start_pass, end_pass,
              charset, pass_len_str, worker_id_str, NULL);
        perror("execl");
        exit(1);
    } else if (pid < 0) {
        perror("fork");
        exit(1);
    }
}

// no processo pai
for (int i = 0; i < num_workers; i++) {
    wait(NULL);
}

```

---

## 3. Comunicação Entre Processos

**Como você garantiu que apenas um worker escrevesse o resultado?**

[Explique como você implementou uma escrita atômica e como isso evita condições de corrida]
Leia sobre condições de corrida (aqui)[https://pt.stackoverflow.com/questions/159342/o-que-%C3%A9-uma-condi%C3%A7%C3%A3o-de-corrida]

**Como o coordinator consegue ler o resultado?**

[Explique como o coordinator lê o arquivo de resultado e faz o parse da informação]

---

## 4. Análise de Performance
Complete a tabela com tempos reais de execução:
O speedup é o tempo do teste com 1 worker dividido pelo tempo com 4 workers.

| Teste | 1 Worker | 2 Workers | 4 Workers | Speedup (4w) |
|-------|----------|-----------|-----------|--------------|
| Hash: 202cb962ac59075b964b07152d234b70<br>Charset: "0123456789"<br>Tamanho: 3<br>Senha: "123" | ___s | ___s | ___s | ___ |
| Hash: 5d41402abc4b2a76b9719d911017c592<br>Charset: "abcdefghijklmnopqrstuvwxyz"<br>Tamanho: 5<br>Senha: "hello" | ___s | ___s | ___s | ___ |

**O speedup foi linear? Por quê?**
[Analise se dobrar workers realmente dobrou a velocidade e explique o overhead de criar processos]

---

## 5. Desafios e Aprendizados
**Qual foi o maior desafio técnico que você enfrentou?**
[Descreva um problema e como resolveu. Ex: "Tive dificuldade com o incremento de senha, mas resolvi tratando-o como um contador em base variável"]

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
