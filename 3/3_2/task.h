#ifndef TASK_H
#define TASK_H

/* Function prototypes to be used by the scheduler */
void data_processing(int **x, int **y);
void perform_matrix_row(int row, int **x, int **y, int **z);

#endif