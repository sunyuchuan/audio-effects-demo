#include <stdio.h>
#include <sys/time.h>
#include "effects.h"
#include "log.h"

int main(int argc, char **argv) {
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_SCREEN);

    int ret = 0;
    short buffer[] = {0, 1, 2, 3};
    int buffer_size = 4;
    int nb_args = 4;
    char *args[10] = {"-t", "10", "-m", "12"};
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);

    EffectContext *ctx = create_effect(find_effect("echo"));
    ret = init_effect(ctx, nb_args, args);
    ret = send_samples(ctx, buffer, buffer_size);
    ret = receive_samples(ctx, buffer, buffer_size);
    for (int i = 0; i < buffer_size; ++i) {
        LogInfo("%d\n", buffer[i]);
    }
    if (ctx) {
        free_effect(ctx);
    }
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);
    return 0;
}