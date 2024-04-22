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

void cleanup(int signum) {
    for (int i = 0; i < N; i++) {
        semctl(global_data->semaphores[i], 0, IPC_RMID); // удаляем семафоры
        semctl(global_data->call_semaphores[i], 0, IPC_RMID); // удаляем семафоры звонков
    }

    // отключаем общую область памяти
    shmdt(global_data);

    // удаляем общую область памяти
    shmctl(shmget(1234, sizeof(struct shared_data), 0666), IPC_RMID, NULL);

    exit(signum);
}

void chatterbox(int id, struct shared_data *data) {
    struct sembuf sem_op;

    while (1) {
        sleep(rand() % 5); // ждем случайное время

        if (!data->busy[id]) { // делаем звонок только если сейчас не на звонке
            int call = rand() % 2; // решаем, делать ли звонок или нет

            if (call) {
                int callee;
                do {
                    callee = rand() % N; // выбираем болтуну, которому позвонить
                } while (callee == id || data->busy[callee]); // если выбранный болтун занят, выбираем другого

                data->busy[id] = 1; // помечаем болтун как занятый
                data->busy[callee] = 1; // помечаем вызываемого как занятый

                printf("Болтун %d звонит болтуну %d\n", id, callee);

                sem_op.sem_num = 0;
                sem_op.sem_op = 1;
                sem_op.sem_flg = 0;
                semop(data->call_semaphores[callee], &sem_op, 1); // сигнализируем вызываемому о входящем звонке

                sleep(rand() % 5); // имитируем звонок

                data->busy[id] = 0; // помечаем болтун как свободный
                data->busy[callee] = 0; // помечаем вызываемого как свободный

                printf("Болтун %d закончил звонить болтуну %d\n", id, callee);

                semop(data->semaphores[callee], &sem_op, 1); // завершаем звонок
            } else {
                printf("Болтун %d ждет звонка...\n", id);
                sleep(rand() % 5); // имитируем ожидание звонка
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <shm_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shm_id = atoi(argv[1]);

    struct shared_data *data = shmat(shm_id, NULL, 0);
    if (data == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // сохраняем указатель на общую область памяти в глобальной переменной
    global_data = data;

    // устанавливаем обработчик сигналов
    signal(SIGINT, cleanup);

    for (int i = 0; i < N; i++) {
        if (data->semaphores[i] != -1) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                chatterbox(i, data);
                exit(EXIT_SUCCESS);
            }
        }
    }

    for (int i = 0; i < N; i++) {
        if (data->semaphores[i] != -1) {
            wait(NULL); // ждем завершения всех дочерних процессов
        }
    }

    return 0;
}