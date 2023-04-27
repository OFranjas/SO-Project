# Objetivos Hoje

- [x] Internal Queue adicionar (sensor reader e user console)
- [ ] Dispatcher dar pop

# Bonus

- [ ] Enviar para worker por unnamed pipe

# Ajuda (Gameiro)

## Função para ignorar todos os sinais

```
void ignore_signals()
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    int i;
    for (i = 1; i <= 64; i++)
    {
        if (i == SIGINT || i == SIGTSTP)
        {
            continue;
        }
        if (sigaction(i, &sa, NULL) == -1)
        {
            // Ignorar sinais que não são suportados
            if (errno == EINVAL)
            {
                continue;
            }
            perror("Erro ao definir acao do sinal");
            exit(EXIT_FAILURE);
        }
    }

    printf("Todos os sinais, exceto SIGINT e SIGTSTP, serao ignorados.\n");

    return;
}

```

## Estrutura da internal queue

```
struct InternalQueueNode
{
    char sensor_id[BUFFER_SIZE];
    char key[BUFFER_SIZE];
    int value;

    char command[BUFFER_SIZE];

    int priority;
    struct InternalQueueNode *next;
};
```
