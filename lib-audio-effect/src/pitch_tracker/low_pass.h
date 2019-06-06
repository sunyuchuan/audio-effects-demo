#ifndef LOW_PASS_H_
#define LOW_PASS_H_

#ifdef __cplusplus
extern "C" {
#endif

void LowPassIIR(const float *in, const short in_len, float *out);

#ifdef __cplusplus
}
#endif
#endif // LOW_PASS_H_
