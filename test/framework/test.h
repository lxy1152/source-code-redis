#ifndef _TEST_H
#define _TEST_H

#define COLOR_NONE "\033[0m"
#define COLOR_RED "\033[0;31m"
#define COLOR_BLUE "\033[1;34m"

/**
 * 记录测试失败的用例个数
 */
static int totalFailedTestsNumber = 0;

static int preTestFailedNumber = 0;

/**
 * 记录总测试用例个数
 */
static int totalTestNumber = 0;

/**
 * 当前测试用例的名称
 */
static char *currentTestName = "";

/**
 * 判断给定的两个值是否相同
 *
 * @param testName 用例名称
 * @param result 函数返回结果
 * @param expected 预期结果
 */
void assertEqual(char *testName, char * description, size_t result, size_t expected);

/**
 * 判断给定的两个值是否不同
 *
 * @param testName 用例名称
 * @param result 函数返回结果
 * @param expected 预期结果
 */
void assertNotEqual(char *testName, char * description, size_t result, size_t expected);

/**
 * 打印测试结果
 */
void printTestReport();

#endif //_TEST_H
