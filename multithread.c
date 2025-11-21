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

#define SHM_SIZE 2048
#define BUFFER_SIZE 512
#define SHM_NAME "/my_shm"
#define SEM_NAME "/my_sem"

// Double buffer structure
typedef struct {
    char buffer_a[BUFFER_SIZE];
    char buffer_b[BUFFER_SIZE];
    int active_write_buffer;  // 0 = buffer_a, 1 = buffer_b
    int write_ready;          // Flag: 1 when write buffer is ready for reading
} double_buffer_t;

void *thread_func(void *arg) {
    int shm_fd;
    double_buffer_t *db;
    sem_t *sem;
    int iteration = 0;

    // Open shared memory
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        pthread_exit(NULL);
    }

    // Map shared memory
    db = (double_buffer_t *)mmap(0, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
    if (db == MAP_FAILED) {
        perror("mmap");
        pthread_exit(NULL);
    }

    // Open semaphore
    sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        pthread_exit(NULL);
    }

    // Write to multiple buffers in a loop (double buffering)
    for (int i = 0; i < 5; i++) {
        iteration++;
        
        // Lock semaphore before accessing shared structure
        sem_wait(sem);
        
        // Determine which buffer to write to
        int write_idx = db->active_write_buffer;
        char *target_buffer = (write_idx == 0) ? db->buffer_a : db->buffer_b;
        
        // Write to the active write buffer
        snprintf(target_buffer, BUFFER_SIZE, "Hello from thread! Iteration: %d, Buffer: %s", 
                 iteration, (write_idx == 0) ? "A" : "B");
        
        // Mark buffer as ready for reading
        db->write_ready = 1;
        
        // Switch to the other buffer for next write (inside lock for safety)
        db->active_write_buffer = 1 - db->active_write_buffer;
        
        // Unlock semaphore - signal that buffer is ready
        sem_post(sem);
        
        usleep(200000); // Small delay to simulate work
    }

    // Clean up
    munmap(db, SHM_SIZE);
    close(shm_fd);

    pthread_exit(NULL);
}

int main() {
    pthread_t thread;
    int shm_fd;
    double_buffer_t *db;
    sem_t *sem;
    int read_count = 0;

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
    db = (double_buffer_t *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (db == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Initialize double buffer structure
    memset(db, 0, sizeof(double_buffer_t));
    db->active_write_buffer = 0;  // Start writing to buffer A
    db->write_ready = 0;

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

    // Read from buffers as they become ready (double buffering)
    while (read_count < 5) {
        // Lock semaphore
        sem_wait(sem);
        
        // Check if a buffer is ready for reading
        if (db->write_ready) {
            // Read from the buffer that was just written
            // Since writer switches buffer after writing, we read from the opposite of current write buffer
            int read_idx = 1 - db->active_write_buffer;
            char *source_buffer = (read_idx == 0) ? db->buffer_a : db->buffer_b;
            
            // Read from shared memory
            printf("Main thread reads: %s\n", source_buffer);
            
            // Mark buffer as read
            db->write_ready = 0;
            read_count++;
        }
        
        // Unlock semaphore
        sem_post(sem);
        
        usleep(100000); // Small delay to allow writer to work
    }

    // Wait for thread to finish
    pthread_join(thread, NULL);

    // Cleanup
    munmap(db, SHM_SIZE);
    close(shm_fd);
    shm_unlink(SHM_NAME);
    sem_close(sem);
    sem_unlink(SEM_NAME);

    return 0;
}
