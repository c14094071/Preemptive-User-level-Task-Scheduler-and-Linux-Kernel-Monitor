#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include "3_2_Config.h"
#include <sys/time.h>
#include <signal.h>    // For signals
#include <sys/time.h>  // For timer

// Task States
#define READY 0
#define RUNNING 1
#define TERMINATED 2

#define matrix_row_x 1234
#define matrix_col_x 250
#define matrix_row_y 250
#define matrix_col_y 1234

FILE *fptr1, *fptr2, *fptr3;
int **x, **y, **z;

int current_row = 0;           
int shared_progress = 0;       
pthread_mutex_t task_lock = PTHREAD_MUTEX_INITIALIZER;     
pthread_mutex_t progress_lock = PTHREAD_MUTEX_INITIALIZER; 

typedef struct {
    int tid;                // Logical Task ID [cite: 154]
    pthread_t p_tid;        // Actual Pthread ID
    int state;              // READY, RUNNING, or TERMINATED [cite: 251]
    int current_row;        // Computation progress
    int priority;           // For Priority scheduling 
    char name[16];          // Task name [cite: 60]
    pthread_cond_t cond;    // Control pause/resume
    pthread_mutex_t mutex;  // Protect TCB access
} TCB;

// TCB 運作流程圖 (以矩陣運算為例)

//     初始化：為每個矩陣區段建立 TCB，設定 current_row 起始值 。

//     執行中：任務不斷更新 TCB 裡的 current_row 。

//     調度發生：

//         暫存：調度器修改 TCB 的 state 為 READY 。

//         恢復：調度器挑選下一個 TCB，根據其 current_row 恢復運算 。

//     結束：當 current_row 到達目標值，TCB 標記為 TERMINATED 。
TCB tcb_pool[20];           // Support up to 20 threads
int active_tasks = 0;
int current_idx = 0;        // Currently running task index
char *sched_algo;           // FCFS, RR, or PP

void data_processing(void){
    int tmp1, tmp2;
    if (fscanf(fptr1, "%d %d", &tmp1, &tmp2) != 2) return;
    for(int i=0; i<matrix_row_x; i++){
        for(int j=0; j<matrix_col_x; j++){
            fscanf(fptr1, "%d", &x[i][j]);
        }
    }
    if (fscanf(fptr2, "%d %d", &tmp1, &tmp2) != 2) return;
    for(int i=0; i<matrix_row_y; i++){
        for(int j=0; j<matrix_col_y; j++){
            fscanf(fptr2, "%d", &y[i][j]);
        }
    }   
}
void *thread_func(void *arg) {
    TCB *my_tcb = (TCB *)arg;

    while (1) {
        // --- Scheduler Control Point ---
        pthread_mutex_lock(&my_tcb->mutex);
        while (my_tcb->state != RUNNING && my_tcb->state != TERMINATED) {
            // Wait until the scheduler signals this thread to run 
            pthread_cond_wait(&my_tcb->cond, &my_tcb->mutex);
        }
        
        if (my_tcb->current_row >= matrix_row_x) {
            my_tcb->state = TERMINATED; // 
            pthread_mutex_unlock(&my_tcb->mutex);
            break;
        }
        
        int row = my_tcb->current_row;
        my_tcb->current_row++;
        pthread_mutex_unlock(&my_tcb->mutex);

        // Matrix Calculation (Keep your original logic)
        for (int j = 0; j < matrix_col_y; j++) {
            z[row][j] = 0;
            for (int k = 0; k < matrix_row_y; k++) {
                z[row][j] += x[row][k] * y[k][j];
            }
        }
    }
    return NULL;
}
void scheduler_handler(int signum) {
    // Round Robin Implementation [cite: 54]
    pthread_mutex_lock(&tcb_pool[current_idx].mutex);
    if (tcb_pool[current_idx].state == RUNNING) {
        tcb_pool[current_idx].state = READY; // Pause current [cite: 261]
    }
    pthread_mutex_unlock(&tcb_pool[current_idx].mutex);

    // Pick next READY task, skip the terminated task
    int next_idx = (current_idx + 1) % active_tasks;
    while (tcb_pool[next_idx].state == TERMINATED) {
        next_idx = (next_idx + 1) % active_tasks;
    }

    current_idx = next_idx;
    pthread_mutex_lock(&tcb_pool[current_idx].mutex);
    tcb_pool[current_idx].state = RUNNING; // Resume next 
    pthread_cond_signal(&tcb_pool[current_idx].cond); // Wake up the thread
    pthread_mutex_unlock(&tcb_pool[current_idx].mutex);

    // Report to Kernel Module [cite: 998, 1053]
    char msg[128];
    sprintf(msg, "SCHEDULER: %s is now running", tcb_pool[current_idx].name);
    int fd = open("/proc/Mythread_info", O_WRONLY);
    if(fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}
// void *thread_func(void *arg) {
//     char *name = (char *)arg;
//     int row;

//     while (1) {
//         pthread_mutex_lock(&task_lock);
//         if (current_row >= matrix_row_x) {
//             pthread_mutex_unlock(&task_lock);
//             break; 
//         }
//         row = current_row;
//         current_row++;
//         pthread_mutex_unlock(&task_lock);

//         for (int j = 0; j < matrix_col_y; j++) {
//             z[row][j] = 0; 
//             for (int k = 0; k < matrix_row_y; k++) {
//                 z[row][j] += x[row][k] * y[k][j];
//             }
//         }

//         pthread_mutex_lock(&progress_lock);
//         shared_progress++;
//         if (shared_progress % 100 == 0) {
//             printf("[%s] Progress: %d/%d\n", name, shared_progress, matrix_row_x);
//         }
//         pthread_mutex_unlock(&progress_lock);
//     }
    
//     // --- 重要：在 Thread 結束前，正確回報與讀取核心訊息 ---
//     char msg[64];
//     sprintf(msg, "%s finished computing.", name);
//     int fd = open("/proc/Mythread_info", O_WRONLY);
//     if(fd >= 0) {
//         write(fd, msg, strlen(msg));
//         close(fd);
//     }

//     // 重新開啟檔案讀取，確保 current->pid 識別正確
//     FILE *fptr_kernel = fopen("/proc/Mythread_info", "r");
//     if (fptr_kernel) {
//         char buffer[256];
//         printf("\n--- Kernel Message for %s ---\n", name);
//         while (fgets(buffer, sizeof(buffer), fptr_kernel) != NULL){
//             printf("%s", buffer);
//         }
//         fclose(fptr_kernel);
//     }
    
//     return NULL;
// }

int main(){
    // 1. Initializations (Malloc, Data Processing)
    sched_algo = (argc > 1) ? argv[1] : "RR"; // Get algorithm from command line
    // 2. Setup Signal Handler [cite: 133]
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &scheduler_handler;
    sigaction(SIGVTALRM, &sa, NULL);
    // 3. Create Threads but keep them in READY state
    active_tasks = THREAD_NUMBER;
    for (int i = 0; i < active_tasks; i++) {
        tcb_pool[i].tid = i + 1;
        tcb_pool[i].state = READY; // Initially all ready [cite: 257]
        pthread_cond_init(&tcb_pool[i].cond, NULL);
        pthread_mutex_init(&tcb_pool[i].mutex, NULL);
        pthread_create(&tcb_pool[i].p_tid, NULL, thread_func, &tcb_pool[i]);
    }
    // 4. Start Timer (10ms interval) 
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 10000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 10000;
    setitimer(ITIMER_VIRTUAL, &timer, NULL);
    // 5. Join Threads and display results
    for (int i = 0; i < active_tasks; i++) {
        pthread_join(tcb_pool[i].p_tid, NULL);
    }
    // 記憶體分配
    x = malloc(sizeof(int*)*matrix_row_x);
    for(int i=0; i<matrix_row_x; i++) x[i] = malloc(sizeof(int)*matrix_col_x);
    y = malloc(sizeof(int*)*matrix_row_y);
    for(int i=0; i<matrix_row_y; i++) y[i] = malloc(sizeof(int)*matrix_col_y);
    z = malloc(sizeof(int*)*matrix_row_x);
    for(int i=0; i<matrix_row_x; i++) {
        z[i] = malloc(sizeof(int)*matrix_col_y);
        memset(z[i], 0, sizeof(int)*matrix_col_y); // 確保初始化為 0
    }

    fptr1 = fopen("m1.txt", "r");
    fptr2 = fopen("m2.txt", "r");
    fptr3 = fopen("3_2.txt", "w"); // 用 "w" 覆寫避免檔案無限增長

    if(!fptr1 || !fptr2 || !fptr3) {
        printf("Error opening matrix files!\n");
        return -1;
    }

    data_processing();
    fprintf(fptr3, "%d %d\n", matrix_row_x, matrix_col_y);
    // start timer
    struct timeval start, end;
    gettimeofday(&start, NULL);
//     pthread_t t1, t2;
    
//     // 根據 Config 決定啟動幾個執行緒
//     pthread_create(&t1, NULL, thread_func, "Thread 1");
// #if (THREAD_NUMBER == 2)
//     pthread_create(&t2, NULL, thread_func, "Thread 2");
// #endif

//     pthread_join(t1, NULL);
// #if (THREAD_NUMBER == 2)
//     pthread_join(t2, NULL);
// #endif
    // 3_2.c main 函式中
    int num_threads = THREAD_NUMBER; 
    pthread_t threads[num_threads];

    for (long i = 0; i < num_threads; i++) {
        char *t_name = malloc(16);
        sprintf(t_name, "Thread %ld", i + 1);
        pthread_create(&threads[i], NULL, thread_func, t_name);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    // --- stop timer ---
    gettimeofday(&end, NULL);
    double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    printf("\n[Summary] Total Multi-threading Execution Time: %.6f seconds\n", time_taken);
    // 寫入結果
    for(int i=0; i<matrix_row_x; i++){
        for(int j=0; j<matrix_col_y; j++){
            fprintf(fptr3, "%d ", z[i][j]);
        }
        fprintf(fptr3, "\n");
    }

    fclose(fptr1);
    fclose(fptr2);
    fclose(fptr3);
    printf("Matrix multiplication finished.\n");
    return 0;
}