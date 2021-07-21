#include "stdio.h"
#include "sds.h"
#include "sds_test.h"
#include "string.h"
#include "test.h"

/**
 * 打印 sds 头信息
 *
 * @param string 一个指向 buf 数组的指针
 */
void printSdsHdrInfo(sds string) {
    struct sdshdr *sh = (void *) (string - (sizeof(struct sdshdr)));
    printf("--------- output sdshdr info start ---------\n");
    printf("len: %u\n", sh->len);
    printf("free: %u\n", sh->free);
    printf("buf: %s\n", sh->buf);
    printf("--------- output sdshdr info end ---------\n");
}

/**
 * sdsnewlen 函数测试
 */
void sdsNewLenTest() {
    char *testName = "sdsNewLenTest";
    sds string = sdsnewlen("redis", 10);
    assertEqual(testName, "compare len", sdslen(string), 10);
    assertEqual(testName, "compare free", sdsavail(string), 0);
    assertEqual(testName, "compare sds using strcmp", strcmp(string, "redis"), 0);
    assertNotEqual(testName, "compare sds using sdscmp", sdscmp(string, "redis"), 0);
}

/**
 * sdsempty 函数测试
 */
void sdsEmptyTest() {
    char *testName = "sdsEmptyTest";
    sds string = sdsempty();
    assertEqual(testName, "compare len", sdslen(string), 0);
    assertEqual(testName, "compare free", sdsavail(string), 0);
    assertEqual(testName, "compare sds using strcmp", strcmp(string, ""), 0);
}

/**
 * sdsnew 函数测试
 */
void sdsNewTest() {
    char *testName = "sdsNewTest";
    sds string = sdsnew("redis");
    assertEqual(testName, "compare len", sdslen(string), 5);
    assertEqual(testName, "compare free", sdsavail(string), 0);
    assertEqual(testName, "compare sds using strcmp", strcmp(string, "redis"), 0);
}

/**
 * sdsdup 函数测试
 */
void sdsDupTest() {
    char *testName = "sdsDupTest";
    sds string = sdsnew("redis");
    sds copy = sdsdup(string);
    assertNotEqual(testName, "compare sds's copy'", (int *) copy, (int *) string);
}

/**
 * sdsupdatelen 函数测试
 */
void sdsUpdateLenTest() {
    char *testName = "sdsUpdateLenTest";
    sds string = sdsnew("redis");
    // 手动修改 buf 数组, 此时 len 没有更新所有会输出 5
    string[2] = '\0';
    assertEqual(testName, "compare len before update", sdslen(string), 5);
    // 更新 len 和 free 后, 将输出 2
    sdsupdatelen(string);
    assertEqual(testName, "compare len after update", sdslen(string), 2);
}

/**
 * sdsclear 函数测试
 */
void sdsClearTest() {
    char *testName = "sdsClearTest";
    sds string = sdsnew("redis");
    sdsclear(string);
    assertEqual(testName, "compare len", sdslen(string), 0);
    assertEqual(testName, "compare free", sdsavail(string), 5);

    struct sdshdr *sh = (void *) (string - sizeof(struct sdshdr));
    char expected[5] = "r\0dis";
    for (int i = 0; i < sdsavail(string); ++i) {
        char *description = "";
        sprintf(description, "compare buf[%d]", i);
        assertEqual(testName, description, sh->buf[0], expected[i]);
    }
}

int main() {
    // 测试用例
    sdsEmptyTest();
    sdsNewLenTest();
    sdsUpdateLenTest();
    sdsNewTest();
    sdsDupTest();
    sdsClearTest();

    // 输出测试结果
    printTestReport();
}
