/* 
 * Name:Taekang Eom, POVIS ID:tkeom0114
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include <math.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
int min(int m, int n);

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
    int m_cache;
    int n_cache;
    int temp;
    if(M == 32)
    {
        m_cache = 8;
        n_cache = 8;
        for(int n = 0; n < N; n+=n_cache)
        {
            for(int m = 0; m < M; m+=m_cache)
            {
                for(int i = n; i < min(n + n_cache ,N); i++)
                {
                    for(int j = m; j < min(m + m_cache,M); j++)
                    {
                        if(i != j)
                        {
                            B[j][i] = A[i][j];
                        }
                        else
                        {
                            temp = A[i][j];
                        }
                        
                    }
                    if(n == m)
                    {
                        B[i][i] = temp;
                    }
                }
            }
        } 
    }
    else if(M == 64)
    {
        m_cache = 8;
        n_cache = 8;
        int a,b,c,d,e,f,g,h;
        for(int n = 0; n < N; n+=n_cache)
        {
            for(int m = 0; m < M; m+=m_cache)
            {
                for(int i = n; i < min(n + n_cache ,N); i+=2)
                {
                    a = A[i][m];
                    b = A[i][m+1];
                    c = A[i][m+2];
                    d = A[i][m+3];
                    e = A[i+1][m];
                    f = A[i+1][m+1];
                    g = A[i+1][m+2];
                    h = A[i+1][m+3];
                    B[m][i] = a;
                    B[m][i+1] = e;
                    B[m+1][i] = b;
                    B[m+1][i+1] = f;
                    B[m+2][i] = c;
                    B[m+2][i+1] = g;
                    B[m+3][i] = d;
                    B[m+3][i+1] = h;
                }
                for(int i = n+6; i >= n; i-=2)
                {
                    a = A[i][m+4];
                    b = A[i][m+5];
                    c = A[i][m+6];
                    d = A[i][m+7];
                    e = A[i+1][m+4];
                    f = A[i+1][m+5];
                    g = A[i+1][m+6];
                    h = A[i+1][m+7];
                    B[m+4][i] = a;
                    B[m+4][i+1] = e;
                    B[m+5][i] = b;
                    B[m+5][i+1] = f;
                    B[m+6][i] = c;
                    B[m+6][i+1] = g;
                    B[m+7][i] = d;
                    B[m+7][i+1] = h;
                }
            }
        } 
    }
    else
    {
        m_cache = 16;
        n_cache = 16;
        for(int n = 0; n < N; n+=n_cache)
        {
            for(int m = 0; m < M; m+=m_cache)
            {
                for(int i = n; i < min(n + n_cache ,N); i++)
                {
                    for(int j = m; j < min(m + m_cache,M); j++)
                    {
                        if(i != j)
                        {
                            B[j][i] = A[i][j];
                        }
                        else
                        {
                            temp = A[i][j];
                        }
                        
                    }
                    if(n == m)
                    {
                        B[i][i] = temp;
                    }
                }
            }
        } 
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

int min(int m, int n)
{
    return m > n ? n:m;
}

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

