/*
 * test_main.c - host test runner.
 *
 *   test_host.exe            run everything
 *   test_host.exe gauge_math  run only suites whose name contains the filter
 */
#include <stdio.h>

#include "test_framework.h"

int main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    printf("round-1.28-gauge host test runner\n");
    return tf_run_all(filter) == 0 ? 0 : 1;
}
