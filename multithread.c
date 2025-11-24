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

int main() {
    int shm_fd;
    int *shm_addr;
    sem_t *sem1;
    sem_t *sem2;
    sem_t *sem3;
    sem_t *sem4;
    pid_td pid;
    int x = 10;
    sem_unlink(SEM_NAME1);
    sem_unlink(SEM_NAME2);
    sem_unlink(SEM_NAME3);
    sem_unlink(SEM_NAME4);
    
    // Create shared memory
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // Set size of shared memory
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

    // Create semaphores
    sem1 = sem_open(SEM_NAME1, O_CREAT | O_RDWR, 0666, 0);
    sem2 = sem_open(SEM_NAME2, O_CREAT | O_RDWR, 0666, 0);
    sem3 = sem_open(SEM_NAME3, O_CREAT | O_RDWR, 0666, 0);
    sem4 = sem_open(SEM_NAME4, O_CREAT | O_RDWR, 0666, 0);
    if (sem1 == SEM_FAILED || sem2 == SEM_FAILED || sem3 == SEM_FAILED || sem4 == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Fork a child process
    int sval;
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    for (int i = 0; i < 10; i++) {
        // child process
        if (i % 2 == 0) {
            sem_wait(sem2);
            *shm_addr = x + i;
            //sleep(1);
            sem_post(sem1);
            printf("wrote buffer 0\n");
        } else {
            //printf("I am at sem_wait(sem4)\n");
            sem_getvalue(sem4, &sval);
            //printf("sem4 value: %d\n", sval);
            sem_wait(sem4);
            *(shm_addr+32*sizeof(int)) = x + i;
            //sleep(1);
            sem_post(sem3);
            printf("wrote buffer 1\n");
        }
        sleep(1);

        //exit(0);
    } else {
        // parent process
        if (i > 0) {
            if(i % 2 == 0) {
                //printf("I am at sem_wait(sem3)\n");
                sem_wait(sem3);
                printf("Parent reads buffer 1:%d\n\n", *(shm_addr+32*sizeof(int)));
                //sleep(1);
                sem_post(sem4);
            } else {
                sem_wait(sem1);
                printf("Parent reads buffer 0: %d\n\n", *shm_addr);
                //sleep(1);
                sem_post(sem2);
            }
        }
        sleep(1);
    }
}