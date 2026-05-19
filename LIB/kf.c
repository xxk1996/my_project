#include "kf.h"
#include "math.h"

float KF_X(float acce_Y, float acce_Z, float gyro_X) 
{
    static float x_hat[2][1] = {0}; 
    static float x_hat_minus[2][1] = {0}; 
    static float p_hat[2][2] = {{1, 0}, {0, 1}}; 
    static float p_hat_minus[2][2] = {0}; 
    static float K[2][1] = {0}; 
    const float Ts = 0.005; // (5ms)
    const float I[2][2] = {{1, 0}, {0, 1}};
    float u[1][1] = {{gyro_X}};
    float A[2][2] = {{1, -Ts}, {0, 1}}; 
    float B[2][1] = {{Ts}, {0}}; 
    float C[1][2] = {{1, 0}};
    float Q[2][2] = {{1e-10, 0}, {0, 1e-10}}; 
    float R[1][1] = {{1e-4}}; 
    float A_T[2][2] = {{1, 0}, {-Ts, 1}}; 
    float C_T[2][1] = {{1}, {0}}; 
    float temp_1[2][1] = {0}; 
    float temp_2[2][1] = {0}; 
    float temp_3[2][2] = {0}; 
    float temp_4[2][2] = {0}; 
    float temp_5[1][2] = {0}; 
    float temp_6[1][1] = {0}; 
    float y = atan2(-acce_Y, acce_Z); 
    // 
    // 
    mul(2, 2, 2, 1, A, x_hat, temp_1);
    mul(2, 1, 1, 1, B, u, temp_2);
    x_hat_minus[0][0] = temp_1[0][0] + temp_2[0][0];
    x_hat_minus[1][0] = temp_1[1][0] + temp_2[1][0];
    // 
    mul(2, 2, 2, 2, A, p_hat, temp_3);
    mul(2, 2, 2, 2, temp_3, A_T, temp_4);
    p_hat_minus[0][0] = temp_4[0][0] + Q[0][0];
    p_hat_minus[0][1] = temp_4[0][1] + Q[0][1];
    p_hat_minus[1][0] = temp_4[1][0] + Q[1][0];
    p_hat_minus[1][1] = temp_4[1][1] + Q[1][1];
    // 
    //
    mul(1, 2, 2, 2, C, p_hat_minus, temp_5);
    mul(1, 2, 2, 1, temp_5, C_T, temp_6);
    temp_6[0][0] = 1.0f / (temp_6[0][0] + R[0][0]);
    mul(2, 2, 2, 1, p_hat_minus, C_T, temp_1);
    mul(2, 1, 1, 1, temp_1, temp_6, K);
    // 
    mul(1, 2, 2, 1, C, x_hat_minus, temp_6);
    temp_6[0][0] = y - temp_6[0][0];
    mul(2, 1, 1, 1, K, temp_6, temp_1);
    x_hat[0][0] = x_hat_minus[0][0] + temp_1[0][0];
    x_hat[1][0] = x_hat_minus[1][0] + temp_1[1][0];
    // 
    mul(2, 1, 1, 2, K, C, temp_3);
    temp_3[0][0] = I[0][0] - temp_3[0][0];
    temp_3[0][1] = I[0][1] - temp_3[0][1];
    temp_3[1][0] = I[1][0] - temp_3[1][0];
    temp_3[1][1] = I[1][1] - temp_3[1][1];
    mul(2, 2, 2, 2, temp_3, p_hat_minus, p_hat);
    // ·
    return x_hat[0][0];
}

/**************************************************************************

**************************************************************************/
float KF_Y(float acce_X, float acce_Z, float gyro_Y) 
{
    static float x_hat[2][1] = {0}; 
    static float x_hat_minus[2][1] = {0}; 
    static float p_hat[2][2] = {{1, 0}, {0, 1}}; 
    static float p_hat_minus[2][2] = {0}; //
    static float K[2][1] = {0}; // 
    const float Ts = 0.005; //
    const float I[2][2] = {{1, 0}, {0, 1}};
    float u[1][1] = {{gyro_Y}};
    float A[2][2] = {{1, -Ts}, {0, 1}}; // 
    float B[2][1] = {{Ts}, {0}}; // 
    float C[1][2] = {{1, 0}};// 
    float Q[2][2] = {{1e-10, 0}, {0, 1e-10}}; // 
    float R[1][1] = {{1e-4}}; // 
    float A_T[2][2] = {{1, 0}, {-Ts, 1}}; // 
    float C_T[2][1] = {{1}, {0}}; // 
    float temp_1[2][1] = {0}; // 
    float temp_2[2][1] = {0}; //
    float temp_3[2][2] = {0}; // 
    float temp_4[2][2] = {0}; // 
    float temp_5[1][2] = {0}; // 
    float temp_6[1][1] = {0}; // 
    float y = atan2(-acce_X, acce_Z); //
    //
    //
    mul(2, 2, 2, 1, A, x_hat, temp_1);
    mul(2, 1, 1, 1, B, u, temp_2);
    x_hat_minus[0][0] = temp_1[0][0] + temp_2[0][0];
    x_hat_minus[1][0] = temp_1[1][0] + temp_2[1][0];
    // 
    mul(2, 2, 2, 2, A, p_hat, temp_3);
    mul(2, 2, 2, 2, temp_3, A_T, temp_4);
    p_hat_minus[0][0] = temp_4[0][0] + Q[0][0];
    p_hat_minus[0][1] = temp_4[0][1] + Q[0][1];
    p_hat_minus[1][0] = temp_4[1][0] + Q[1][0];
    p_hat_minus[1][1] = temp_4[1][1] + Q[1][1];
    // 
    // 
    mul(1, 2, 2, 2, C, p_hat_minus, temp_5);
    mul(1, 2, 2, 1, temp_5, C_T, temp_6);
    temp_6[0][0] = 1.0f / (temp_6[0][0] + R[0][0]);
    mul(2, 2, 2, 1, p_hat_minus, C_T, temp_1);
    mul(2, 1, 1, 1, temp_1, temp_6, K);
    // 
    mul(1, 2, 2, 1, C, x_hat_minus, temp_6);
    temp_6[0][0] = y - temp_6[0][0];
    mul(2, 1, 1, 1, K, temp_6, temp_1);
    x_hat[0][0] = x_hat_minus[0][0] + temp_1[0][0];
    x_hat[1][0] = x_hat_minus[1][0] + temp_1[1][0];
    // 
    mul(2, 1, 1, 2, K, C, temp_3);
    temp_3[0][0] = I[0][0] - temp_3[0][0];
    temp_3[0][1] = I[0][1] - temp_3[0][1];
    temp_3[1][0] = I[1][0] - temp_3[1][0];
    temp_3[1][1] = I[1][1] - temp_3[1][1];
    mul(2, 2, 2, 2, temp_3, p_hat_minus, p_hat);
    // 
    return x_hat[0][0];
}

/**************************************************************************

**************************************************************************/
void mul(int A_row, int A_col, int B_row, int B_col, float A[][A_col], float B[][B_col], float C[][B_col])
{
    if (A_col == B_row)
    {
        for (int i = 0; i < A_row; i++)
        {
            for (int j = 0; j < B_col; j++)
            {
                C[i][j] = 0; 
                for (int k = 0; k < A_col; k++)
                {
                    C[i][j] += A[i][k]*B[k][j];
                }
            }
        }
    }
    else
    {
       
    }
}



