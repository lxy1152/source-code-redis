#ifndef _TEST_H
#define _TEST_H

#define COLOR_NONE "\033[0m"
#define COLOR_RED "\033[0;31m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_YELLOW "\033[1;33m"

/**
 * 记录测试失败的总个数
 */
static int totalFailedTestsNumber = 0;

/**
 * 某个测试的测试用例失败的个数
 */
static int preTestFailedNumber = 0;

/**
 * 记录总测试个数
 */
static int totalTestNumber = 0;

/**
 * 当前测试的名称
 */
static char *currentTestName = "";

/**
 * 判断给定的两个数值是否相同
 *
 * @param testName 用例名称
 * @param description 描述信息
 * @param result 结果
 * @param expected 预期结果
 */
void assertEqualForNumber(char *testName, char *description, size_t result, size_t expected);

/**
 * 判断给定的两个数值是否不同
 *
 * @param testName 用例名称
 * @param description 描述信息
 * @param result 结果
 * @param expected 预期结果
 */
void assertNotEqualForNumber(char *testName, char *description, size_t result, size_t expected);

/**
 * 判断给定的两个 char 型指针是否相同
 *
 * @param testName 用例名称
 * @param description 描述信息
 * @param result 结果
 * @param expected 预期结果
 */
void assertEqualForString(char *testName, char *description, const char *result, const char *expected);

/**
 * 判断给定的两个 char 型指针是否不同
 *
 * @param testName 用例名称
 * @param description 描述信息
 * @param result 结果
 * @param expected 预期结果
 */
void assertNotEqualForString(char *testName, char *description, const char *result, const char *expected);

void assertEqualForSds(char *testName, char *description, sds result, sds expected);

void assertNotEqualForSds(char *testName, char *description, sds result, sds expected);

/**
 * 打印测试结果
 */
void printTestReport();

#endif //_TEST_H
