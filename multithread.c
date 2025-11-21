/***********************************************************
* Filename: multithread.c
* Description: using pthread
* Author: Bob Turney
* Date: 3/24/2024
* Note: gcc -o multithread multithread.c -pthread -lrt
***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

#define SHM_SIZE 1024
#define SHM_NAME "/my_shm"
#define SEM_NAME "/my_sem"

void *thread_func(void *arg) {
    int shm_fd;
    char *shm_addr;
    sem_t *sem;

    // Open shared memory
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        pthread_exit(NULL);
    }

    // Map shared memory
    shm_addr = mmap(0, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        pthread_exit(NULL);
    }

    // Open semaphore
    sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        pthread_exit(NULL);
    }

    // Lock semaphore
    sem_wait(sem);

    // Write to shared memory
    snprintf(shm_addr, SHM_SIZE, "Hello from thread!");

    // Unlock semaphore
    sem_post(sem);

    // Clean up
    munmap(shm_addr, SHM_SIZE);
    close(shm_fd);

    pthread_exit(NULL);
}

int main() {
    pthread_t thread;
    int shm_fd;
    char *shm_addr;
    sem_t *sem;

    // Create shared memory
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // Set the size of the shared memory
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    // Map the shared memory
    shm_addr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Create semaphore
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Create thread
    if (pthread_create(&thread, NULL, thread_func, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    // Wait for thread to finish
    pthread_join(thread, NULL);

    // Lock semaphore
    sem_wait(sem);

    // Read from shared memory
    printf("Main thread reads: %s\n", shm_addr);

    // Unlock semaphore
    sem_post(sem);

    // Cleanup
    munmap(shm_addr, SHM_SIZE);
    close(shm_fd);
    shm_unlink(SHM_NAME);
    sem_close(sem);
    sem_unlink(SEM_NAME);

    return 0;
}
