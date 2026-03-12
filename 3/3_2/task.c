#include <stdio.h>
#include <stdlib.h>
#include "task.h"

/* Dimensions must match your input data files */
#define matrix_row_x 1234
#define matrix_col_x 250
#define matrix_row_y 250
#define matrix_col_y 1234

/* Load matrix data from files into memory */
void data_processing(int **x, int **y) {
    FILE *fptr1 = fopen("m1.txt", "r");
    FILE *fptr2 = fopen("m2.txt", "r");
    int tmp1, tmp2;

    if (!fptr1 || !fptr2) {
        perror("Error opening input matrix files");
        exit(EXIT_FAILURE);
    }

    /* Read header and data for Matrix X */
    if (fscanf(fptr1, "%d %d", &tmp1, &tmp2) == 2) {
        for (int i = 0; i < matrix_row_x; i++) {
            for (int j = 0; j < matrix_col_x; j++) {
                fscanf(fptr1, "%d", &x[i][j]);
            }
        }
    }

    /* Read header and data for Matrix Y */
    if (fscanf(fptr2, "%d %d", &tmp1, &tmp2) == 2) {
        for (int i = 0; i < matrix_row_y; i++) {
            for (int j = 0; j < matrix_col_y; j++) {
                fscanf(fptr2, "%d", &y[i][j]);
            }
        }
    }

    fclose(fptr1);
    fclose(fptr2);
}

/* Calculate a single row of the result matrix Z */
void perform_matrix_row(int row, int **x, int **y, int **z) {
    for (int j = 0; j < matrix_col_y; j++) {
        z[row][j] = 0;
        for (int k = 0; k < matrix_row_y; k++) {
            /* Standard matrix multiplication: Z[i][j] = sum(X[i][k] * Y[k][j]) */
            z[row][j] += x[row][k] * y[k][j];
        }
    }
}