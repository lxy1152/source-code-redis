#include "assert.h"
#include "stdio.h"
#include "sds.h"
#include "sds_test.h"
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
    printf("========= sdsEmptyTest start =========\n");
    sds string = sdsempty();
    printSdsHdrInfo(string);
    printf("========= sdsEmptyTest end =========\n\n");
}

/**
 * sdsnewlen 函数测试
 */
void sdsNewLenTest() {
    sds string = sdsnewlen("redis", 10);
    testCond("sdsnewlen(\"redis\", 10)", sdscmp(string, "redis") != 0);
}

int main() {
    // sdsEmptyTest();
    sdsNewLenTest();
    printTestReport();
}
