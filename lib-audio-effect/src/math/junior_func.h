#ifndef JUNIOR_FUNCTIONS_H_
#define JUNIOR_FUNCTIONS_H_

static __inline float _reciprocal_sqrt(float x) {
    const float threeHalves = 1.5f;
    float number, xhalf, xcopy, tmp1, tmp2, tmp3, tmp4, tmp5;
    int xbitcopy;

    number = x;
    xhalf = 0.5f * x;
    xbitcopy = *(int*)&number;
    xbitcopy = 0x5f3759df - (xbitcopy >> 1);
    xcopy = *(float*)&xbitcopy;
    tmp1 = xcopy * xcopy;
    tmp2 = xcopy * threeHalves;
    tmp3 = xcopy * xhalf;
    tmp4 = tmp2 - tmp1 * tmp3;
    tmp5 = tmp4 * tmp4;
    tmp2 = tmp4 * threeHalves;
    tmp3 = tmp4 * xhalf;

    return (tmp2 - tmp5 * tmp3);
}
float round_float(float in);

#endif