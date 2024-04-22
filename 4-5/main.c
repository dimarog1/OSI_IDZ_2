#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <signal.h>

#define N 5 // количество болтунов


struct shared_data {
    sem_t semaphores[N];
    sem_t call_semaphores[N]; // дополнительный массив семафоров для сигнализации о входящих звонках
    int busy[N]; // массив для отслеживания, находится ли каждый болтун на звонке или нет
};

struct shared_data *global_data = NULL;
pid_t pids[N] = {0};

void cleanup(int signum) {
    for (int i = 0; i < N; i++) {
        if (pids[i] != 0) {
            kill(pids[i], SIGKILL); // завершаем все дочерние процессы
        }
        sem_destroy(&global_data->semaphores[i]); // уничтожаем семафоры
        sem_destroy(&global_data->call_semaphores[i]); // уничтожаем семафоры звонков
    }

    // отключаем общую область памяти
    munmap(global_data, sizeof(struct shared_data));

    exit(signum);
}

void chatterbox(int id, struct shared_data *data) {
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

                sem_post(&data->call_semaphores[callee]); // сигнализируем вызываемому о входящем звонке

                sleep(rand() % 5); // имитируем звонок

                data->busy[id] = 0; // помечаем болтун как свободный
                data->busy[callee] = 0; // помечаем вызываемого как свободный

                printf("Болтун %d закончил звонить болтуну %d\n", id, callee);

                sem_post(&data->semaphores[callee]); // завершаем звонок
            } else {
                printf("Болтун %d ждет звонка...\n", id);
                sleep(rand() % 5); // имитируем ожидание звонка
            }
        }
    }
}

int main() {
    srand(time(NULL));

    printf("Количество болтунов: %d\n", N);

    // создаем общую область памяти для структуры shared_data
    struct shared_data *data = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; i++) {
        sem_init(&data->semaphores[i], 1, 1); // инициализируем семафор для каждого болтуна
        sem_init(&data->call_semaphores[i], 1, 0); // инициализируем семафор для каждого болтуна для сигнализации о входящих звонках
    }

    // сохраняем указатель на общую область памяти в глобальной переменной
    global_data = data;

    // устанавливаем обработчик сигналов
    signal(SIGINT, cleanup);

    for (int i = 0; i < N; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            chatterbox(i, data);
            exit(EXIT_SUCCESS);
        } else {
            pids[i] = pid; // сохраняем PID дочернего процесса в глобальном массиве
        }
    }

    for (int i = 0; i < N; i++) {
        wait(NULL); // ждем завершения всех болтунов
    }

    return 0;
}