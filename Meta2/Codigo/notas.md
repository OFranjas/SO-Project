# Objetivos Hoje

- [ ] Key Queue -> DEU MERDA, cada worker parece ter a sua shared memory
- [ ] Printar lista de keys
- [x] # Message Queue Workers
- [ ] Key Queue
- [ ] Printar lista de keys


Para ver se workers tao livres
Printar comando errado

# Codigo Gameiro
```
sudo mount -t drvfs C: /mnt/c -o metadata,uid=1000,gid=1000,umask=22,fmask=111
```

# Falta

<<<<<<< HEAD

- [ ] # Alerts Watcher
- [ ]

# Extra Bonus

- [ ]

# Ajuda (Gameiro)

## Comando para pipes

```
sudo mount -t drvfs C: /mnt/c -o metadata,uid=1000,gid=1000,umask=22,fmask=111
```

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
