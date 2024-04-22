#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>
#include <signal.h>

#define N 5 // количество болтунов

struct shared_data {
    int semaphores[N];
    int call_semaphores[N]; // дополнительный массив семафоров для сигнализации о входящих звонках
    int busy[N]; // массив для отслеживания, находится ли каждый болтун на звонке или нет
};

struct shared_data *global_data = NULL;
pid_t pids[N] = {0};

void cleanup(int signum) {
    for (int i = 0; i < N; i++) {
        if (pids[i] != 0) {
            kill(pids[i], SIGKILL); // завершаем все дочерние процессы
        }
        if (pids[i] != -1) {
            kill(pids[i], SIGTERM); // отправляем сигнал SIGTERM всем дочерним процессам
        }
        semctl(global_data->semaphores[i], 0, IPC_RMID); // удаляем семафоры
        semctl(global_data->call_semaphores[i], 0, IPC_RMID); // удаляем семафоры звонков
    }

    // отключаем общую область памяти
    shmdt(global_data);

    // удаляем общую область памяти
    shmctl(shmget(1234, sizeof(struct shared_data), 0666), IPC_RMID, NULL);

    exit(signum);
}

int main() {
    srand(time(NULL));

    printf("Количество болтунов: %d\n", N);

    // создаем общую область памяти для структуры shared_data
    int shm_id = shmget(1234, sizeof(struct shared_data), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    struct shared_data *data = shmat(shm_id, NULL, 0);
    if (data == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; i++) {
        data->semaphores[i] = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666); // инициализируем семафор для каждого болтуна
        data->call_semaphores[i] = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666); // инициализируем семафор для каждого болтуна для сигнализации о входящих звонках
    }

    printf("Сервер запущен. ID общей области памяти: %d\n", shm_id);

    // сохраняем указатель на общую область памяти в глобальной переменной
    global_data = data;

    // устанавливаем обработчик сигналов
    signal(SIGINT, cleanup);

    while (1) {
        pause(); // ждем сигналов от клиентов
    }

    return 0;
}