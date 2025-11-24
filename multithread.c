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
#include <math.h>

#define SHM_NAME "/my_shm"
#define SEM_NAME1 "/my_sem1"
#define SEM_NAME2 "/my_sem2"
#define SEM_NAME3 "/my_sem3"
#define SEM_NAME4 "/my_sem4"
#define SHM_SIZE 1024

typedef struct {
    int* shm_addr;

    // buffer 1 sempahores
    sem_t* sem_write_mem1_0;
    sem_t* sem_write_mem1_1;
    sem_t* sem_read_mem1_0;
    sem_t* sem_read_mem1_1;

    // buffer 2 semaphores
    sem_t* sem_write_mem2_0;
    sem_t* sem_write_mem2_1;
    sem_t* sem_read_mem2_0;
    sem_t* sem_read_mem2_1;
} thread_data_t;

// thread function declarations
void* thread1(void* arg);
void* thread2(void* arg);
void* thread3(void* arg);

int main() {
    // Initialize double buffer
    int shm_fd;
    int *shm_addr;

    // Initialize semaphores
    sem_t sem_write_mem1_0;
    sem_t sem_write_mem1_1;
    sem_t sem_read_mem1_0;
    sem_t sem_read_mem1_1;
    sem_t sem_write_mem2_0;
    sem_t sem_write_mem2_1;
    sem_t sem_read_mem2_0;
    sem_t sem_read_mem2_1;

    // Initialize x value for calculations
    int x = 10;

    // unlink existing shared memory
    shm_unlink(SHM_NAME);

    // Create shared memory and map
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    shm_addr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // initialize buffer 1 sempahores
    sem_init(&sem_write_mem1_0, 0, 1);
    sem_init(&sem_write_mem1_1, 0, 1);
    sem_init(&sem_read_mem1_0, 0, 0);
    sem_init(&sem_read_mem1_1, 0, 0);

    // initialize buffer 2 semaphores
    sem_init(&sem_write_mem2_0, 0, 1);
    sem_init(&sem_write_mem2_1, 0, 1);
    sem_init(&sem_read_mem2_0, 0, 0);
    sem_init(&sem_read_mem2_1, 0, 0);

    thread_data_t thread_data = {
        shm_addr,
        &sem_write_mem1_0,
        &sem_write_mem1_1,
        &sem_read_mem1_0,
        &sem_read_mem1_1,
        &sem_write_mem2_0,
        &sem_write_mem2_1,
        &sem_read_mem2_0,
        &sem_read_mem2_1,
    };

    // Note:
    // Buffer 0: *shm_addr
    // Buffer 1: *(shm_addr + 32 * sizeof(int))

    // create 3 threads, one for each calculation
    pthread_t thread_sq;
    pthread_t thread_add;
    pthread_t thread_sqrt;
    pthread_create(&thread_sq, NULL, thread1, &thread_data);
    pthread_create(&thread_add, NULL, thread2, &thread_data);
    pthread_create(&thread_sqrt, NULL, thread3, &thread_data);

    pthread_join(thread_sq, NULL);
    pthread_join(thread_add, NULL);
    pthread_join(thread_sqrt, NULL);

    munmap(shm_addr, SHM_SIZE);
    close(shm_fd);
    shm_unlink(SHM_NAME);
    sem_destroy(&sem_write_mem1_0);
    sem_destroy(&sem_write_mem1_1);
    sem_destroy(&sem_read_mem1_0);
    sem_destroy(&sem_read_mem1_1);
    sem_destroy(&sem_write_mem2_0);
    sem_destroy(&sem_write_mem2_1);
    sem_destroy(&sem_read_mem2_0);
    sem_destroy(&sem_read_mem2_1);

    // return to exit
    return 0;
}

void* thread1(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int* shm_addr = data->shm_addr;
    double x = 10;

    for (int i = 0; i < 10; i++) {
        // claculate result
        double result = x * x;

        // write to buffer
        if (i % 2 == 0) { // filters out half of the threads
            sem_wait(data->sem_write_mem1_0);
            *shm_addr = result;
            sem_post(data->sem_read_mem1_0);
        } else {
            sem_wait(data->sem_write_mem1_1);
            *(shm_addr + 32 * sizeof(int)) = result;
            sem_post(data->sem_read_mem1_1);
        }

        // sleep for 1 second
        sleep(1);

        // add 1 to x
        x = x + 1;
    }
    return NULL;
}

void* thread2(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int* shm_addr = data->shm_addr;
    
    for (int i = 0; i < 10; i++) {
        double input; // grabbed value from buffer

        // read from buffer
        if (i % 2 == 0) {
            sem_wait(data->sem_read_mem1_0);
            input = *shm_addr;
            sem_post(data->sem_write_mem1_0);
        } else {
            sem_wait(data->sem_read_mem1_1);
            input = *(shm_addr + 32 * sizeof(int));
            sem_post(data->sem_write_mem1_1);
        }

        // calculate +10
        double result = input + 10;

        // sleep for 1 second
        sleep(1);

        // write to second buffer
        if (i % 2 == 0) {
            sem_wait(data->sem_write_mem2_0);
            *(shm_addr + 64 * sizeof(int)) = result;
            sem_post(data->sem_read_mem2_0);
        } else {
            sem_wait(data->sem_write_mem2_1);
            *(shm_addr + 96 * sizeof(int)) = result;
            sem_post(data->sem_read_mem2_1);
        }
    }
    return NULL;
}

void* thread3(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int* shm_addr = data->shm_addr;

    for (int i = 0; i < 10; i++) {
        double input; // grabbed value from second buffer

        // read from second buffer
        if (i % 2 == 0) {
            sem_wait(data->sem_read_mem2_0);
            input = *(shm_addr + 64 * sizeof(int));
            sem_post(data->sem_write_mem2_0);
        } else {
            sem_wait(data->sem_read_mem2_1);
            input = *(shm_addr + 96 * sizeof(int));
            sem_post(data->sem_write_mem2_1);
        }

        // calculate sqrt
        double result = sqrt(input);

        // sleep for 1 second
        sleep(1);

        //print out result
        printf("Result %d: %f\n", i+1,result);
    }
    return NULL;
}