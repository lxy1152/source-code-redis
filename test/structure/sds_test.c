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
    assertEqualForNumber(testName, "compare len", sdslen(string), 10);
    assertEqualForNumber(testName, "compare free", sdsavail(string), 0);
    assertEqualForNumber(testName, "compare sds using strcmp", strcmp(string, "redis"), 0);
    assertNotEqualForNumber(testName, "compare sds using sdscmp", sdscmp(string, "redis"), 0);
}

/**
 * sdsempty 函数测试
 */
void sdsEmptyTest() {
    char *testName = "sdsEmptyTest";
    sds string = sdsempty();
    assertEqualForNumber(testName, "compare len", sdslen(string), 0);
    assertEqualForNumber(testName, "compare free", sdsavail(string), 0);
    assertEqualForNumber(testName, "compare sds using strcmp", strcmp(string, ""), 0);
}

/**
 * sdsnew 函数测试
 */
void sdsNewTest() {
    char *testName = "sdsNewTest";
    sds string = sdsnew("redis");
    assertEqualForNumber(testName, "compare len", sdslen(string), 5);
    assertEqualForNumber(testName, "compare free", sdsavail(string), 0);
    assertEqualForNumber(testName, "compare sds using strcmp", strcmp(string, "redis"), 0);
}

/**
 * sdsdup 函数测试
 */
void sdsDupTest() {
    char *testName = "sdsDupTest";
    sds string = sdsnew("redis");
    sds copy = sdsdup(string);
    assertNotEqualForString(testName, "compare sds's copy'", copy, string);
}

/**
 * sdsupdatelen 函数测试
 */
void sdsUpdateLenTest() {
    char *testName = "sdsUpdateLenTest";
    sds string = sdsnew("redis");
    // 手动修改 buf 数组, 此时 len 没有更新所有会输出 5
    string[2] = '\0';
    assertEqualForNumber(testName, "compare len before update", sdslen(string), 5);
    // 更新 len 和 free 后, 将输出 2
    sdsupdatelen(string);
    assertEqualForNumber(testName, "compare len after update", sdslen(string), 2);
}

/**
 * sdsclear 函数测试
 */
void sdsClearTest() {
    char *testName = "sdsClearTest";
    sds string = sdsnew("redis");
    sdsclear(string);
    assertEqualForNumber(testName, "compare len", sdslen(string), 0);
    assertEqualForNumber(testName, "compare free", sdsavail(string), 5);

    struct sdshdr *sh = (void *) (string - (sizeof(struct sdshdr)));
    char expected[5] = "\0edis";
    size_t free = sh->free;
    int digit = 0;
    while (free != 0) {
        digit++;
        free = free / 10;
    }
    for (int i = 0; i < sh->free; ++i) {
        char description[13 + digit];
        sprintf(description, "compare buf[%d]", i);
        assertEqualForNumber(testName, description, sh->buf[i], expected[i]);
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
