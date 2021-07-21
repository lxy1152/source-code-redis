#include <stdlib.h>
#include "test.h"
#include "stdio.h"
#include "string.h"

/**
 * 对某个测试用例 testName 判断条件 expression 是否成立
 *
 * @param testName 测试用例名称
 * @param expression 判断条件
 */
void testInCondition(char *testName, char *description, int expression) {
    if (strcmp(testName, currentTestName) != 0) {
        currentTestName = testName;
        if (preTestFailedNumber > 0) {
            totalFailedTestsNumber++;
        }
        preTestFailedNumber = 0;
        totalTestNumber++;
        printf("%d - %s: \n", totalTestNumber, testName);
    }
    printf("  - %s: ", description);
    if (expression) {
        printf(COLOR_BLUE"PASSED\n"COLOR_NONE);
    } else {
        preTestFailedNumber++;
        printf(COLOR_RED"FAILED\n"COLOR_NONE);
    }
}

/**
 * 判断给定的两个值是否相同
 *
 * @param testName 用例名称
 * @param result 函数返回结果
 * @param expected 预期结果
 */
void assertEqual(char *testName, char *description, size_t result, size_t expected) {
    testInCondition(testName, description, result == expected);
}

/**
 * 判断给定的两个值是否不同
 *
 * @param testName 用例名称
 * @param result 函数返回结果
 * @param expected 预期结果
 */
void assertNotEqual(char *testName, char *description, size_t result, size_t expected) {
    testInCondition(testName, description, result != expected);
}

/**
 * 打印测试结果
 */
void printTestReport() {
    if (preTestFailedNumber > 0) {
        totalFailedTestsNumber++;
        preTestFailedNumber = 0;
    }
    printf("\n%d tests, %d passed, %d failed\n",
           totalTestNumber,
           totalTestNumber - totalFailedTestsNumber,
           totalFailedTestsNumber);
    if (totalFailedTestsNumber) {
        printf(COLOR_RED"======== WARNING ========\nWe have failed ");
        if (totalFailedTestsNumber > 1) {
            printf("tests ");
        } else {
            printf("test ");
        }
        printf("here"COLOR_NONE);
        exit(1);
    }
}
