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
        struct sdshdr *sh = getSdsHdr(string);
        printf(COLOR_YELLOW"  - print sdshdr info before %s: len: %u, free: %u, buf: %s\n"COLOR_NONE,
               description,
               sh->len,
               sh->free,
               sh->buf);
    }
}

/**
 * 获取指定 sds 的头信息
 *
 * @param string sds
 * @return 头信息
 */
struct sdshdr *getSdsHdr(sds string) {
    return (void *) (string - (sizeof(struct sdshdr)));
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
    assertNotEqualForNumber(testName, "compare sds's copy'", copy, string);
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

    struct sdshdr *sh = getSdsHdr(string);
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

/**
 * sdsIncrLen 函数测试
 */
void sdsIncrLenTest() {
    char *testName = "sdsIncrLenTest";
    sds string = sdsnew("redis");

    // 要往 sds 中追加的字符串
    char buffer[] = {'1', '2', '3', '4', '5'};
    // 数组长度
    int bufferSize = sizeof(buffer) / sizeof(buffer[0]);
    // sds 扩容
    string = sdsMakeRoomFor(string, bufferSize);

    struct sdshdr *sdshdr = getSdsHdr(string);

    assertEqualForString(testName, "compare buf before append", sdshdr->buf, "redis");
    // 追加字符串
    int start = 5;
    for (int i = start; i < start + bufferSize; ++i) {
        *(string + i) = buffer[i - start];
    }
    assertEqualForString(testName, "compare buf after append", sdshdr->buf, "redis12345");

    // 比较增加前与增加后的 sds 长度
    assertEqualForNumber(testName, "compare len before using sdsIncrLen to increase", sdshdr->len, 5);
    sdsIncrLen(string, bufferSize);
    assertEqualForNumber(testName, "compare len after using sdsIncrLen to increase", sdshdr->len, 10);

    // 因为减少前的长度上面已经展示过了, 所以不需要重复判断
    // 比较减少后的 sds 长度
    sdsIncrLen(string, -bufferSize);
    assertEqualForNumber(testName, "compare len after using sdsIncrLen to decrease", sdshdr->len, 5);
}

/**
 * sdsgrowzero 函数测试
 */
void sdsGrowZeroTest() {
    char *testName = "sdsGrowZeroTest";

    sds string = sdsnew("redis");

    struct sdshdr *sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len before grow", sdshdr->len, 5);
    assertEqualForNumber(testName, "compare free before grow", sdshdr->free, 0);
    string = sdsgrowzero(string, 10);
    sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len after grow", sdshdr->len, 10);
    assertEqualForNumber(testName, "compare free after grow", sdshdr->free, 10);
}

/**
 * sdscatlen 函数测试
 */
void sdsCatLenTest() {
    char *testName = "sdsCatLenTest";

    sds string = sdsnew("redis");

    struct sdshdr *sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len before using sdscatlen", sdshdr->len, 5);
    assertEqualForNumber(testName, "compare free before using sdscatlen", sdshdr->free, 0);
    string = sdscatlen(string, "123456", 5);
    sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len after using sdscatlen", sdshdr->len, 10);
    assertEqualForNumber(testName, "compare free after using sdscatlen", sdshdr->free, 10);

    assertEqualForString(testName, "compare buf before using sdscatlen", sdshdr->buf, "redis12345");
    string = sdscatlen(string, "abc\0d", 5);
    sdshdr = getSdsHdr(string);
    assertEqualForString(testName, "compare buf after using sdscatlen", sdshdr->buf, "redis12345abc");
}

/**
 * sdscat 函数测试
 */
void sdsCatTest() {
    char *testName = "sdsCatTest";

    sds string = sdsnew("redis");

    struct sdshdr *sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len before using sdscat", sdshdr->len, 5);
    assertEqualForNumber(testName, "compare free before using sdscat", sdshdr->free, 0);
    string = sdscat(string, "12345\0bc");
    sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len after using sdscat", sdshdr->len, 10);
    assertEqualForNumber(testName, "compare free after using sdscat", sdshdr->free, 10);
}

/**
 * sdscatsds 函数测试
 */
void sdsCatSdsTest() {
    char *testName = "sdsCatSdsTest";

    sds string = sdsnew("redis");

    struct sdshdr *sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len before using sdscatsds", sdshdr->len, 5);
    assertEqualForNumber(testName, "compare free before using sdscatsds", sdshdr->free, 0);
    string = sdscatsds(string, sdsnewlen("12345", 10));
    sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len after using sdscatsds", sdshdr->len, 15);
    assertEqualForNumber(testName, "compare free after using sdscatsds", sdshdr->free, 15);
}

/**
 * sdscpylen 函数测试
 */
void sdsCpyLenTest() {
    char *testName = "sdsCpyLenTest";

    sds string = sdsnew("redis");

    struct sdshdr *sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len before using sdscpylen", sdshdr->len, 5);
    assertEqualForNumber(testName, "compare free before using sdscpylen", sdshdr->free, 0);
    assertEqualForString(testName, "compare buf before using sdscpylen", sdshdr->buf, "redis");
    string = sdscpylen(string, "0123456789", 10);
    sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len after using sdscpylen", sdshdr->len, 10);
    assertEqualForNumber(testName, "compare free after using sdscpylen", sdshdr->free, 10);
    assertEqualForString(testName, "compare buf after using sdscpylen", sdshdr->buf, "0123456789");
}

/**
 * sdscpy 函数测试
 */
void sdsCpyTest() {
    char *testName = "sdsCpyTest";

    sds string = sdsnew("redis");

    struct sdshdr *sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len before using sdscpy", sdshdr->len, 5);
    assertEqualForNumber(testName, "compare free before using sdscpy", sdshdr->free, 0);
    assertEqualForString(testName, "compare buf before using sdscpy", sdshdr->buf, "redis");
    string = sdscpy(string, "0123456789\0abc");
    sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len after sdscpy", sdshdr->len, 10);
    assertEqualForNumber(testName, "compare free after sdscpy", sdshdr->free, 10);
    assertEqualForString(testName, "compare buf after sdscpy", sdshdr->buf, "0123456789");
}

void sdsFromLongLongTest() {
    char *testName = "sdsFromLongLongTest";

    long long value = 2147483648;
    sds string = sdsfromlonglong(value);

    struct sdshdr *sdshdr = getSdsHdr(string);
    assertEqualForNumber(testName, "compare len after creating from long long value", sdshdr->len, 10);
    assertEqualForNumber(testName, "compare free after creating from long long value", sdshdr->free, 0);
    assertEqualForString(testName, "compare buf after creating from long long value", sdshdr->buf, "2147483648");
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
    sdsIncrLenTest();
    sdsGrowZeroTest();
    sdsCatLenTest();
    sdsCatTest();
    sdsCatSdsTest();
    sdsCpyLenTest();
    sdsCpyTest();
    sdsFromLongLongTest();

    // 输出测试结果
    printTestReport();
}
