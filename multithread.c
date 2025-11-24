/***********************************************************
* Filename: multithread.c
* Description: using pthread
* Author: Bob Turney
* Date: 3/24/2024
* Note: gcc -o multithread multithread.c -pthread -lrt -lm
***********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>

#define SHM_NAME "/my_shm"
#define SEM_NAME1 "/my_sem1"
#define SEM_NAME2 "/my_sem2"
#define SEM_NAME3 "/my_sem3"
#define SEM_NAME4 "/my_sem4"
#define SHM_SIZE 1024

typedef struct {
    int* shm_addr;
    sem_t* sem_write_a;
    sem_t* sem_write_b;
    sem_t* sem_read_a;
    sem_t* sem_read_b;
    sem_t* sem_mutex;
} thread_data_t;

void* writer_thread(void* arg);
void* reader_thread(void* arg);

int main() {
    // Initialize double buffer
    int shm_fd;
    int *shm_addr;

    // Initialize semaphores
    sem_t sem_write_a;
    sem_t sem_write_b;
    sem_t sem_read_a;
    sem_t sem_read_b;
    sem_t sem_mutex;

    // Initialize x value for calculations
    int x = 10;

    // unlink existing shared memory
    shm_unlink(SHM_NAME);

    // Create shared memory and map
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    shm_addr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    sem_init(&sem_write_a, 0, 1);
    sem_init(&sem_write_b, 0, 1);
    sem_init(&sem_read_a, 0, 0);
    sem_init(&sem_read_b, 0, 0);
    sem_init(&sem_mutex, 0, 1);

    thread_data_t thread_data = {
        shm_addr,
        &sem_write_a,
        &sem_write_b,
        &sem_read_a,
        &sem_read_b,
        &sem_mutex
    };

    // Note:
    // Buffer 0: *shm_addr
    // Buffer 1: *(shm_addr + 32 * sizeof(int))

    pthread_t writer;
    pthread_t reader;
    pthread_create(&writer, NULL, writer_thread, &thread_data);
    pthread_create(&reader, NULL, reader_thread, &thread_data);

    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    munmap(shm_addr, SHM_SIZE);
    close(shm_fd);
    shm_unlink(SHM_NAME);
    sem_destroy(&sem_write_a);
    sem_destroy(&sem_write_b);
    sem_destroy(&sem_read_a);
    sem_destroy(&sem_read_b);
    sem_destroy(&sem_mutex);

    // return to exit
    return 0;
}

void* writer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int* shm_addr = data->shm_addr;
    int x = 10;

    for (int i = 0; i < 10; i++) {
        if (i % 2 == 0) { // filters out half of the threads
            sem_wait(data->sem_write_a);
            *shm_addr = x + i;
            sem_post(data->sem_read_a);
            printf("wrote buffer 0\n");
        } else {
            sem_wait(data->sem_write_b);
            *(shm_addr + 32 * sizeof(int)) = x + i;
            sem_post(data->sem_read_b);
            printf("wrote buffer 1\n");
        }
        sleep(1);
    }
    return NULL;
}

void* reader_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int* shm_addr = data->shm_addr;
    
    for (int i = 0; i < 10; i++) {
        if (i % 2 == 0) {
            sem_wait(data->sem_read_a);
            printf("Parent reads buffer 0: %d\n\n", *shm_addr);
            sem_post(data->sem_write_a);
        } else {
            sem_wait(data->sem_read_b);
            printf("Parent reads buffer 1: %d\n\n", *(shm_addr + 32 * sizeof(int)));
            sem_post(data->sem_write_b);
        }
        sleep(1);
    }
    return NULL;
}