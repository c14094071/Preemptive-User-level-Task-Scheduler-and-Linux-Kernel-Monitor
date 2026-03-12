#define _GNU_SOURCE 
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include "3_2_Config.h" // Important: Include the config from Makefile 

#define MAX_THREADS 20
#define READY 0
#define RUNNING 1
#define TERMINATED 2

/* Matrix Dimensions */
#define matrix_row_x 1234
#define matrix_col_x 250
#define matrix_row_y 250
#define matrix_col_y 1234

typedef struct {
    int tid;
    pthread_t p_tid;
    int state;
    char name[16];
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} TCB;

TCB tcb_pool[MAX_THREADS];
int active_tasks = 0;
int current_idx = 0;
int current_row = 0;
int **x, **y, **z;

extern void data_processing(int **x, int **y);
extern void perform_matrix_row(int row, int **x, int **y, int **z);

void report_to_kernel(const char* message) {
    int fd = open("/proc/Mythread_info", O_WRONLY);
    if (fd >= 0) {
        write(fd, message, strlen(message));
        close(fd);
    }
}

void check_kernel_status() {
    int fd = open("/proc/Mythread_info", O_RDONLY); // 以唯讀模式開啟
    if (fd >= 0) {
        char buffer[1024];
        int bytes = read(fd, buffer, sizeof(buffer) - 1); // 觸發 Myread
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("\n[Kernel Feedback]\n%s\n", buffer); // 印出核心回傳的監控資訊
        }
        close(fd);
    }
}

void scheduler_handler(int signum) {
    if (pthread_mutex_trylock(&tcb_pool[current_idx].mutex) == 0) {
        if (tcb_pool[current_idx].state == RUNNING) {
            tcb_pool[current_idx].state = READY;
        }
        pthread_mutex_unlock(&tcb_pool[current_idx].mutex);
    } else {
        return; 
    }

    int next_idx = (current_idx + 1) % active_tasks;
    int search_count = 0;
    while (tcb_pool[next_idx].state == TERMINATED && search_count < active_tasks) {
        next_idx = (next_idx + 1) % active_tasks;
        search_count++;
    }

    if (search_count == active_tasks) return;

    
    printf("\n[OS Scheduler] Time Quantum Out. Switching: %s -> %s\n", 
           tcb_pool[current_idx].name, tcb_pool[next_idx].name);
    fflush(stdout);

    current_idx = next_idx;
    if (pthread_mutex_trylock(&tcb_pool[current_idx].mutex) == 0) {
        if (tcb_pool[current_idx].state == READY) {
            tcb_pool[current_idx].state = RUNNING;
            pthread_cond_signal(&tcb_pool[current_idx].cond); 
        }
        pthread_mutex_unlock(&tcb_pool[current_idx].mutex);
    }
}

void *thread_func(void *arg) {
    TCB *my_tcb = (TCB *)arg;
    while (1) {
        pthread_mutex_lock(&my_tcb->mutex);
        while (my_tcb->state != RUNNING && my_tcb->state != TERMINATED) {
            pthread_cond_wait(&my_tcb->cond, &my_tcb->mutex);
        }
        if (my_tcb->state == TERMINATED) {
            pthread_mutex_unlock(&my_tcb->mutex);
            break;
        }
        pthread_mutex_unlock(&my_tcb->mutex);

        
        if (my_tcb->tid == 1) {
            // Task 1: 專注矩陣運算 (CPU Intensive)
            int row = __sync_fetch_and_add(&current_row, 1);
            if (row >= matrix_row_x) {
                my_tcb->state = TERMINATED;
                break;
            }
            perform_matrix_row(row, x, y, z);
            if (row % 100 == 0) {
                printf("[%s] Calculating Row %d...\n", my_tcb->name, row);
                fflush(stdout);
            }
        } else {
            // Task 2: 專注於與核心溝通 (I/O Intensive)
            if (current_row >= matrix_row_x) {
                my_tcb->state = TERMINATED;
                printf("[%s] Work is done, shutting down monitor.\n", my_tcb->name);
                break;
            }
            char msg[64];
            sprintf(msg, "Heartbeat from %s at row %d", my_tcb->name, current_row);
            report_to_kernel(msg);
            // printf("[%s] Sent heartbeat to Kernel Module.\n", my_tcb->name);
            check_kernel_status();
            
            usleep(20000); // 模擬稍微停頓，讓 RR 更容易介入
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    /* Memory allocation and data loading */
    x = malloc(sizeof(int*) * matrix_row_x);
    for(int i=0; i<matrix_row_x; i++) x[i] = malloc(sizeof(int) * matrix_col_x);
    y = malloc(sizeof(int*) * matrix_row_y);
    for(int i=0; i<matrix_row_y; i++) y[i] = malloc(sizeof(int) * matrix_col_y);
    z = malloc(sizeof(int*) * matrix_row_x);
    for(int i=0; i<matrix_row_x; i++) z[i] = calloc(matrix_col_y, sizeof(int));

    data_processing(x, y);

    /* Use SIGALRM and ITIMER_REAL for consistent timing [cite: 132] */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &scheduler_handler;
    sigaction(SIGALRM, &sa, NULL); 

    active_tasks = THREAD_NUMBER;
    for (int i = 0; i < active_tasks; i++) {
        tcb_pool[i].tid = i + 1;
        sprintf(tcb_pool[i].name, "Thread-%d", i+1);
        tcb_pool[i].state = READY;
        pthread_cond_init(&tcb_pool[i].cond, NULL);
        pthread_mutex_init(&tcb_pool[i].mutex, NULL);
        pthread_create(&tcb_pool[i].p_tid, NULL, thread_func, &tcb_pool[i]);
    }

    /* Start first thread */
    pthread_mutex_lock(&tcb_pool[0].mutex);
    tcb_pool[0].state = RUNNING;
    pthread_cond_signal(&tcb_pool[0].cond);
    pthread_mutex_unlock(&tcb_pool[0].mutex);

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 500000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 500000;
    setitimer(ITIMER_REAL, &timer, NULL); 

    for (int i = 0; i < active_tasks; i++) pthread_join(tcb_pool[i].p_tid, NULL);

    /* Final Output */
    FILE *fout = fopen("3_2.txt", "w");
    fprintf(fout, "%d %d\n", matrix_row_x, matrix_col_y);
    for(int i=0; i<matrix_row_x; i++) {
        for(int j=0; j<matrix_col_y; j++) fprintf(fout, "%d ", z[i][j]);
        fprintf(fout, "\n");
    }
    fclose(fout);
    printf("Result saved. Simulation finished.\n");
    return 0;
}