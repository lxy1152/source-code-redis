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
 * sdsnewlen 函数测试
 */
void sdsNewLenTest() {
    char *testName = "sdsNewLenTest";
    sds string = sdsnewlen("redis", 10);
    assertEqual(testName, "compare len", sdslen(string), 10);
    assertEqual(testName, "compare free", sdsavail(string), 0);
    assertEqual("sdsNewLenTest", "compare sds using strcmp", strcmp(string, "redis"), 0);
    assertNotEqual("sdsNewLenTest", "compare sds using sdscmp", sdscmp(string, "redis"), 0);
}

/**
 * sdsupdatelen 函数测试
 */
void sdsUpdateLenTest() {
    sds string = sdsnew("redis");
    // 手动修改 buf 数组, 此时 len 没有更新所有会输出 5
    string[2] = '\0';
    assertEqual("sdsUpdateLenTest", "compare len before update", sdslen(string), 5);
    // 更新 len 和 free 后, 将输出 2
    sdsupdatelen(string);
    assertEqual("sdsUpdateLenTest", "compare len after update", sdslen(string), 2);
}

int main() {
    // 测试用例
    sdsEmptyTest();
    sdsNewLenTest();
    sdsUpdateLenTest();

    // 输出测试结果
    printTestReport();
}
