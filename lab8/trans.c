/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 * Student Name: Li Ying
 * Student ID: 516030910444
 *
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	int i, j, row, column, temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
	if ( M==32 && N==32)
	{
	/* Since b=5, each cache line can store 8 int. I divide the 32*32 matrix into the 8*8 one
	   so when I get one int the cache line will store the remained 7 int to reduce cache miss. 
	   The special situation is those on the diagonal line which won't change place so I use 
	   temp variable to store them.
	*/
		for (row = 0; row <N; row += 8)
		{
			for (column = 0; column < M; column += 8)
			{
				for (i = row; i < row + 8; i++)
				{
					for (j = column; j < column + 8; j++)
					{
						if( i == j)
						{
							temp0 = A[i][j];
							continue;
						}
						B[j][i] = A[i][j];
					}
					if (row == column)
						B[i][i] = temp0;
				}
			}
		}
	}

	if ( M==64 && N==64)
	{
	/* If I use the same method to solve this situation, it will cause lots of misses since the 5th line
	   is in the same cache line of the 1st line. So I change a method. I use the cache line of B to store
	   the remained 4 int and then move them to the proper place. For example, I use eignt temp variables
	   to store the 8 int in the cache line of A, the first 4 are put to the proper place while the remained
	   ones are temporarily stored in the cache line of B.
	*/
		for (row = 0; row <N; row += 8)
		{
			for (column = 0; column < M; column += 8)
			{
				for (i = row; i < row + 4; i++)
				{
					temp0 = A[i][column];
					temp1 = A[i][column + 1];
					temp2 = A[i][column + 2];
					temp3 = A[i][column + 3];
					temp4 = A[i][column + 4];
					temp5 = A[i][column + 5];
					temp6 = A[i][column + 6];
					temp7 = A[i][column + 7];
					B[column][i] = temp0;
					B[column+1][i] = temp1;
					B[column+2][i] = temp2;
					B[column+3][i] = temp3;
					B[column+3][i+4] = temp4;
					B[column][i+4] = temp5;
					B[column+1][i+4] = temp6;
					B[column+2][i+4] = temp7;
				}
				temp0 = A[row+4][column+4];
				temp1 = A[row+5][column+4];
				temp2 = A[row+6][column+4];
				temp3 = A[row+7][column+4];
				temp4 = A[row+4][column+3];
				temp5 = A[row+5][column+3];
				temp6 = A[row+6][column+3];
				temp7 = A[row+7][column+3];
				B[column+4][row] = B[column+3][row+4];
				B[column+4][row+4] = temp0;
				B[column+4][row+1] = B[column+3][row+5];
				B[column+4][row+5] = temp1;
				B[column+4][row+2] = B[column+3][row+6];
				B[column+4][row+6] = temp2;
				B[column+4][row+3] = B[column+3][row+7];
				B[column+4][row+7] = temp3;
				B[column+3][row+4] = temp4;
				B[column+3][row+5] = temp5;
				B[column+3][row+6] = temp6;
				B[column+3][row+7] = temp7;

				for (i=0; i<3; i++)
				{
					temp0 = A[row+4][column+5+i];
					temp1 = A[row+5][column+5+i];
					temp2 = A[row+6][column+5+i];
					temp3 = A[row+7][column+5+i];
					temp4 = A[row+4][column+i];
					temp5 = A[row+5][column+i];
					temp6 = A[row+6][column+i];
					temp7 = A[row+7][column+i];
					B[column+5+i][row+0] = B[column+i][row+4];
					B[column+5+i][row+4] = temp0;
						
					B[column+5+i][row+1] = B[column+i][row+5];
					B[column+5+i][row+5] = temp1;
				
					B[column+5+i][row+2] = B[column+i][row+6];
					B[column+5+i][row+6] = temp2;

					B[column+5+i][row+3] = B[column+i][row+7];
					B[column+5+i][row+7] = temp3;
	
					B[column+i][row+4] = temp4;
					B[column+i][row+5] = temp5;
					B[column+i][row+6] = temp6;
					B[column+i][row+7] = temp7;
				}

			}
		}
	}

	if ( M==61 && N==67)
	{
	// the same as the 32*32 one and just modify the number from 8 to 16
		for (row = 0; row <N; row += 16)
		{
			for (column = 0; column < M; column += 16)
			{
				for (i = row; (i < row+16) && (i<N); i++)
				{
					for (j = column; (j < column+16) && (j<M); j++)
					{
						if( i == j)
						{
							temp0 = A[i][j];
							continue;
						}
						B[j][i] = A[i][j];
					}
					if (row == column)
						B[i][i] = temp0;
				}
			}
		}
	}

}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
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
void registerFunctions()
{
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
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

