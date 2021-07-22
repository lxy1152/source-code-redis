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
void printSdsHdrInfo(sds string, char *description) {
    if (printFlag) {
        struct sdshdr *sh = (void *) (string - (sizeof(struct sdshdr)));
        printf(COLOR_YELLOW"  - print sdshdr info before %s: len: %u, free: %u, buf: %s\n"COLOR_NONE,
               description,
               sh->len,
               sh->free,
               sh->buf);
    }
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

/**
 * sdsMakeRoomFor 函数测试
 */
void sdsMakeRoomForTest() {
    char *testName = "sdsMakeRoomForTest";
    sds string = sdsnew("redis");
    assertEqualForNumber(testName, "compare len before using makeRoomFor", sdslen(string), 5);
    assertEqualForNumber(testName, "compare free before using makeRoomFor", sdsavail(string), 0);

    // 1M 内扩容
    string = sdsMakeRoomFor(string, 50);
    printSdsHdrInfo(string, "compare data after using makeRoomFor(50)");
    // 扩容不会修改 len, 所以已使用长度是不变的
    assertEqualForNumber(testName, "compare len after using makeRoomFor(50)", sdslen(string), 5);
    // 空闲空间是根据新长度重新计算出来的
    // 因为长度小于 1M, 所以是 2 倍扩容
    assertEqualForNumber(testName, "compare free after using makeRoomFor(50)", sdsavail(string), 105);

    // 大于等于 1M 时扩容
    string = sdsMakeRoomFor(string, 1048571);
    printSdsHdrInfo(string, "compare data after using makeRoomFor(1048571)");
    assertEqualForNumber(testName, "compare len after using makeRoomFor(1048571)", sdslen(string), 5);
    // newlen 走了大于等于 1M 的分支, 所以 newlen 加上了 1048576
    assertEqualForNumber(testName, "compare free after using makeRoomFor(1048571)", sdsavail(string), 2097147);
}

/**
 * sdsRemoveFreeSpace 函数测试
 */
void sdsRemoveFreeSpaceTest() {
    char *testName = "sdsRemoveFreeSpaceTest";
    sds string = sdsnewlen("redis", 10);
    sdsupdatelen(string);
    assertEqualForNumber(testName, "compare free before using sdsRemoveFreeSpace", sdsavail(string), 5);
    string = sdsRemoveFreeSpace(string);
    assertEqualForNumber(testName, "compare free after using sdsRemoveFreeSpace", sdsavail(string), 0);
}

/**
 * sdsAllocSize 函数测试
 */
void sdsAllocSizeTest() {
    char *testName = "sdsRemoveFreeSpaceTest";
    sds string = sdsnewlen("redis", 10);
    sdsupdatelen(string);
    assertEqualForNumber(testName, "compare allocSize", sdsAllocSize(string), 19);
}

int main() {
    // 设置为允许输出调试信息
    setPrintFlag(1);

    // 要执行的测试
    sdsEmptyTest();
    sdsNewLenTest();
    sdsUpdateLenTest();
    sdsNewTest();
    sdsDupTest();
    sdsClearTest();
    sdsMakeRoomForTest();
    sdsRemoveFreeSpaceTest();
    sdsAllocSizeTest();

    // 输出测试结果
    printTestReport();
}
