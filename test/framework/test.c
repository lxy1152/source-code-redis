//
// Created by lxy on 2021-07-20.
//

#include <stdlib.h>
#include "test.h"
#include "stdio.h"

void testCond(char *testName, int expression) {
    totalTestNumber++;
    printf("%d - %s: ", totalTestNumber, testName);
    if (expression) {
        printf("PASSED\n");
    } else {
        failedTestsNumber++;
        printf("FAILED\n");
    }
}

void printTestReport() {
    printf("%d tests, %d passed, %d failed\n", totalTestNumber, totalTestNumber - failedTestsNumber, failedTestsNumber);
    if (failedTestsNumber) {
        printf("=== WARNING === We have failed tests here...\n");
        exit(1);
    }
}
