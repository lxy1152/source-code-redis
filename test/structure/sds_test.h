#ifndef _SDS_TEST_H
#define _SDS_TEST_H

/**
 * 是否输出调试信息
 */
static size_t printFlag = 0;

/**
 * 设置是否输出调试信息
 *
 * @param flag 是否输出
 */
void setPrintFlag(size_t flag);

/**
 * 打印 sds 头信息
 *
 * @param string 一个指向 buf 数组的指针
 */
void printSdsHdrInfo(sds string, char *description);

/**
 * 设置是否输出调试信息
 *
 * @param flag 是否输出
 */
void setPrintFlag(size_t flag) {
    printFlag = flag;
}

#endif //_SDS_TEST_H
