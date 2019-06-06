#include "low_pass.h"

const float a1[3] = {0.04589f, -0.0912487f, 0.04589f};
const float b1[2] = {-1.975961f, 0.9779f};
const float a2[2] = {0.0338585f, 0.0338585f};  // q15,q15
const float b2[1] = {-0.974798f};              // q15,q15

void LowPassIIR(const float *in, const short in_len, float *out) {
    static float sa1[2] = {0.0f, 0.0f};
    static float sb1[2] = {0.0f, 0.0f};
    static float sa2[1] = {0.0f};
    static float sb2[1] = {0.0f};

    float tmpno1, tmpno2, tmpno3, tmpno4;
    for (short i = 0; i < in_len; ++i) {
        tmpno1 = in[i] * a1[0];
        tmpno2 = sa1[0] * a1[1];
        tmpno3 = sa1[1] * a1[2] + tmpno1;
        sa1[1] = sa1[0];
        tmpno4 = tmpno2 - b1[0] * sb1[0];
        sa1[0] = in[i];
        tmpno1 = tmpno3 - b1[1] * sb1[1];
        tmpno2 = tmpno4 + tmpno1;
        sb1[1] = sb1[0];
        tmpno3 = tmpno2 * a2[0];
        tmpno4 = sa2[0] * a2[1] + tmpno3;
        sb1[0] = tmpno2;
        sa2[0] = tmpno2;
        tmpno1 = tmpno4 - sb2[0] * b2[0];
        out[i] = tmpno1;
        sb2[0] = tmpno1;
    }
}