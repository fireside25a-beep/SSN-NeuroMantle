#include <process_runner.h>

#include <assert.h>
#include <stdio.h>

int main(void) {
    char *success[] = {"true", NULL};
    char *failure[] = {"false", NULL};
    char *missing[] = {"neuroroute-command-that-does-not-exist", NULL};

    assert(process_run(NULL, success) == 0);
    assert(process_run(NULL, failure) == 1);
    assert(process_run(NULL, missing) == 127);
    puts("process runner tests: PASS");
    return 0;
}
