#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include "3_2_Config.h"
#include <sys/time.h>
#include <signal.h>

// Task States
#define READY      0
#define RUNNING    1
#define TERMINATED 2

#define matrix_row_x 1234
#define matrix_col_x 250
#define matrix_row_y 250
#define matrix_col_y 1234

FILE *fptr1, *fptr2, *fptr3;
int **x, **y, **z;

pthread_mutex_t task_lock     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t progress_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int         tid;         // Logical Task ID
    pthread_t   p_tid;       // Actual Pthread ID
    int         state;       // READY, RUNNING, or TERMINATED
    int         current_row; // Computation progress
    int         priority;    // For Priority scheduling
    char        name[16];    // Task name
    pthread_cond_t  cond;    // Control pause/resume
    pthread_mutex_t mutex;   // Protect TCB access
} TCB;

TCB  tcb_pool[20];   // Support up to 20 threads
int  active_tasks = 0;
int  current_idx  = 0;  // Currently running task index
char *sched_algo;       // FCFS, RR, or PP

// ─── Data Processing ────────────────────────────────────────────────────────

void data_processing(void) {
    int tmp1, tmp2;

    if (fscanf(fptr1, "%d %d", &tmp1, &tmp2) != 2) return;
    for (int i = 0; i < matrix_row_x; i++)
        for (int j = 0; j < matrix_col_x; j++)
            fscanf(fptr1, "%d", &x[i][j]);

    if (fscanf(fptr2, "%d %d", &tmp1, &tmp2) != 2) return;
    for (int i = 0; i < matrix_row_y; i++)
        for (int j = 0; j < matrix_col_y; j++)
            fscanf(fptr2, "%d", &y[i][j]);
}

// ─── Thread Function ─────────────────────────────────────────────────────────

void *thread_func(void *arg) {
    TCB *my_tcb = (TCB *)arg;

    while (1) {
        pthread_mutex_lock(&my_tcb->mutex);
        while (my_tcb->state != RUNNING && my_tcb->state != TERMINATED)
            pthread_cond_wait(&my_tcb->cond, &my_tcb->mutex);

        if (my_tcb->current_row >= matrix_row_x) {
            my_tcb->state = TERMINATED;
            pthread_mutex_unlock(&my_tcb->mutex);
            break;
        }

        int row = my_tcb->current_row;
        my_tcb->current_row++;
        pthread_mutex_unlock(&my_tcb->mutex);

        for (int j = 0; j < matrix_col_y; j++) {
            z[row][j] = 0;
            for (int k = 0; k < matrix_row_y; k++)
                z[row][j] += x[row][k] * y[k][j];
        }
    }
    return NULL;
}

// ─── Scheduler Signal Handler (Round Robin) ───────────────────────────────────

void scheduler_handler(int signum) {
    // Check if all tasks are terminated
    int all_done = 1;
    for (int i = 0; i < active_tasks; i++) {
        if (tcb_pool[i].state != TERMINATED) {
            all_done = 0;
            break;
        }
    }
    if (all_done) return;

    // Pause current running task
    pthread_mutex_lock(&tcb_pool[current_idx].mutex);
    if (tcb_pool[current_idx].state == RUNNING)
        tcb_pool[current_idx].state = READY;
    pthread_mutex_unlock(&tcb_pool[current_idx].mutex);

    // Pick next READY task, skip terminated ones
    int next_idx = (current_idx + 1) % active_tasks;
    while (tcb_pool[next_idx].state == TERMINATED)
        next_idx = (next_idx + 1) % active_tasks;

    current_idx = next_idx;

    pthread_mutex_lock(&tcb_pool[current_idx].mutex);
    tcb_pool[current_idx].state = RUNNING;
    pthread_cond_signal(&tcb_pool[current_idx].cond);
    pthread_mutex_unlock(&tcb_pool[current_idx].mutex);

    // Report to Kernel Module
    char msg[128];
    sprintf(msg, "SCHEDULER: %s is now running", tcb_pool[current_idx].name);
    int fd = open("/proc/Mythread_info", O_WRONLY);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    sched_algo = (argc > 1) ? argv[1] : "RR";

    // Memory allocation
    x = malloc(sizeof(int *) * matrix_row_x);
    for (int i = 0; i < matrix_row_x; i++)
        x[i] = malloc(sizeof(int) * matrix_col_x);

    y = malloc(sizeof(int *) * matrix_row_y);
    for (int i = 0; i < matrix_row_y; i++)
        y[i] = malloc(sizeof(int) * matrix_col_y);

    z = malloc(sizeof(int *) * matrix_row_x);
    for (int i = 0; i < matrix_row_x; i++) {
        z[i] = malloc(sizeof(int) * matrix_col_y);
        memset(z[i], 0, sizeof(int) * matrix_col_y);
    }

    // Open files
    fptr1 = fopen("m1.txt", "r");
    fptr2 = fopen("m2.txt", "r");
    fptr3 = fopen("3_2.txt", "w");
    if (!fptr1 || !fptr2 || !fptr3) {
        printf("Error opening matrix files!\n");
        return -1;
    }

    data_processing();

    // Setup signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &scheduler_handler;
    sigaction(SIGVTALRM, &sa, NULL);

    // Create threads in READY state
    active_tasks = THREAD_NUMBER;
    for (int i = 0; i < active_tasks; i++) {
        tcb_pool[i].tid         = i + 1;
        tcb_pool[i].state       = READY;
        tcb_pool[i].current_row = 0;
        snprintf(tcb_pool[i].name, sizeof(tcb_pool[i].name), "Thread %d", i + 1);
        pthread_cond_init(&tcb_pool[i].cond, NULL);
        pthread_mutex_init(&tcb_pool[i].mutex, NULL);
        pthread_create(&tcb_pool[i].p_tid, NULL, thread_func, &tcb_pool[i]);
    }

    // Start the first task manually
    pthread_mutex_lock(&tcb_pool[0].mutex);
    tcb_pool[0].state = RUNNING;
    pthread_cond_signal(&tcb_pool[0].cond);
    pthread_mutex_unlock(&tcb_pool[0].mutex);

    // Start virtual timer (10ms interval)
    struct itimerval timer;
    timer.it_value.tv_sec     = 0;
    timer.it_value.tv_usec    = 10000;
    timer.it_interval.tv_sec  = 0;
    timer.it_interval.tv_usec = 10000;
    setitimer(ITIMER_VIRTUAL, &timer, NULL);

    // Start timing
    struct timeval start, end;
    gettimeofday(&start, NULL);

    // Wait for all threads to finish
    for (int i = 0; i < active_tasks; i++)
        pthread_join(tcb_pool[i].p_tid, NULL);

    // Stop timing
    gettimeofday(&end, NULL);
    double time_taken = (end.tv_sec  - start.tv_sec) +
                        (end.tv_usec - start.tv_usec) / 1e6;
    printf("\n[Summary] Total Multi-threading Execution Time: %.6f seconds\n", time_taken);

    // Write result matrix
    fprintf(fptr3, "%d %d\n", matrix_row_x, matrix_col_y);
    for (int i = 0; i < matrix_row_x; i++) {
        for (int j = 0; j < matrix_col_y; j++)
            fprintf(fptr3, "%d ", z[i][j]);
        fprintf(fptr3, "\n");
    }

    fclose(fptr1);
    fclose(fptr2);
    fclose(fptr3);
    printf("Matrix multiplication finished.\n");
    return 0;
}
