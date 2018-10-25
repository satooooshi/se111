/*
 *
 * Student Name:Huichuan Hui
 * Student ID:516413990003
 *
 */

/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include "cachelab.h"
#include <stdio.h>

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

void trans(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
  int t1, t2, t3, t4, t5, t6, t7, t8;
  if (M == 32 && N == 32) {
    for (int i = 0; i < N; i += 8) {
      for (int j = 0; j < M; j += 8) {
        for (int m = i; m < i + 8; m++) {
          int n = j;
          t1 = A[m][n + 0];
          t2 = A[m][n + 1];
          t3 = A[m][n + 2];
          t4 = A[m][n + 3];

          t5 = A[m][n + 4];
          t6 = A[m][n + 5];
          t7 = A[m][n + 6];
          t8 = A[m][n + 7];

          B[n + 0][m] = t1;
          B[n + 1][m] = t2;
          B[n + 2][m] = t3;
          B[n + 3][m] = t4;

          B[n + 4][m] = t5;
          B[n + 5][m] = t6;
          B[n + 6][m] = t7;
          B[n + 7][m] = t8;
        }
      }
    }
  } else if (M == 64 && N == 64) {
    for (int i = 0; i < N; i += 8) {
      for (int j = 0; j < M; j += 8) {
        for (int m = i; m < i + 4; m++) {
          int n = j;
          t1 = A[m][n + 0];
          t2 = A[m][n + 1];
          t3 = A[m][n + 2];
          t4 = A[m][n + 3];
          t5 = A[m][n + 4];
          t6 = A[m][n + 5];
          t7 = A[m][n + 6];
          t8 = A[m][n + 7];

          B[n + 0][m] = t1;
          B[n + 1][m] = t2;
          B[n + 2][m] = t3;
          B[n + 3][m] = t4;

          B[n + 0][m + 4] = t5;
          B[n + 1][m + 4] = t6;
          B[n + 2][m + 4] = t7;
          B[n + 3][m + 4] = t8;
        }
        for (int n = j; n < j + 4; n++) {
          int m = i;
          t1 = A[m + 4][n];
          t2 = A[m + 5][n];
          t3 = A[m + 6][n];
          t4 = A[m + 7][n];

          t5 = B[n][m + 4];
          t6 = B[n][m + 5];
          t7 = B[n][m + 6];
          t8 = B[n][m + 7];

          B[n][m + 4] = t1;
          B[n][m + 5] = t2;
          B[n][m + 6] = t3;
          B[n][m + 7] = t4;

          B[n + 4][m + 0] = t5;
          B[n + 4][m + 1] = t6;
          B[n + 4][m + 2] = t7;
          B[n + 4][m + 3] = t8;
        }

        for (int m = i + 4; m < i + 8; m += 2) {
          int n = j + 4;
          t1 = A[m][n + 0];
          t2 = A[m][n + 1];
          t3 = A[m][n + 2];
          t4 = A[m][n + 3];

          t5 = A[m + 1][n + 0];
          t6 = A[m + 1][n + 1];
          t7 = A[m + 1][n + 2];
          t8 = A[m + 1][n + 3];

          B[n + 0][m] = t1;
          B[n + 1][m] = t2;
          B[n + 2][m] = t3;
          B[n + 3][m] = t4;

          B[n + 0][m + 1] = t5;
          B[n + 1][m + 1] = t6;
          B[n + 2][m + 1] = t7;
          B[n + 3][m + 1] = t8;
        }
      }
    }
  } else if (M == 61 && N == 67) {
    int block_size = 16;
    for (int i = 0; i < N; i += block_size)
      for (int j = 0; j < M; j += block_size)
        for (int m = i; m < i + block_size && m < N; m++)
          for (int n = j; n < j + block_size && n < M; n++)
            B[n][m] = A[m][n];
  } else
    trans(M, N, A, B);

}
/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
  int i;
  int j;
  int buf;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; j++) {
      buf = A[i][j];
      B[j][i] = buf;
    }
  }
}


/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions() {
  /* Register your solution function */
  registerTransFunction(transpose_submit, transpose_submit_desc);

  /* Register any additional transpose functions */
  registerTransFunction(trans, trans_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
  int i;
  int j;

  for (i = 0; i < N; i++)
    for (j = 0; j < M; ++j)
      if (A[i][j] != B[j][i])
        return 0;

  return 1;
}
