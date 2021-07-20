/* SDSLib, A C dynamic strings library
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SDS_H
#define __SDS_H

/**
 * 预分配空间最大值
 */
#define SDS_MAX_PREALLOC (1024*1024)

#include <sys/types.h>
#include <stdarg.h>

/**
 * 对 char* 起了个别名叫 sds, 实际上它是一个指向 buf 数组的指针
 */
typedef char *sds;

/**
 * 保存 sds 头信息
 */
struct sdshdr {
    /**
     * buf 数组中已使用字节的数量, 即 sds 的长度
     */
    unsigned int len;

    /**
     * buf 数组中未使用的字节数量
     */
    unsigned int free;

    /**
     * 用于保存字符串
     */
    char buf[];
};

/**
 * 获取 sds 的实际已使用长度
 *
 * @param s sds
 * @return 已使用长度
 */
static inline size_t sdslen(const sds s) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    return sh->len;
}

/**
 * 获取 sds 的空闲长度
 *
 * @param s sds
 * @return 空闲长度
 */
static inline size_t sdsavail(const sds s) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    return sh->free;
}

/**
 * 根据给定的初始字符串 init 以及初始长度 initlen 创建一个新的 sds
 *
 * @param init 初始字符串
 * @param initlen 初始长度
 * @return 创建好的 sds
 */
sds sdsnewlen(const void *init, size_t initlen);

/**
 * 根据给定的初始字符串 init 创建一个新的 sds
 *
 * @param init 初始字符串
 * @return 创建好的 sds
 */
sds sdsnew(const char *init);

/**
 * 创建一个空的 sds
 *
 * @return 创建好的 sds
 */
sds sdsempty(void);

/**
 * 获取 sds 的实际已使用长度
 *
 * @param s sds
 * @return 已使用长度
 */
size_t sdslen(const sds s);

/**
 * 将给定的 sds 复制, 得到一个新的 sds
 *
 * @param s sds
 * @return 复制后得到的 sds
 */
sds sdsdup(const sds s);

/**
 * 释放给定的 sds
 *
 * @param s sds
 */
void sdsfree(sds s);

/**
 * 获取 sds 的空闲长度
 *
 * @param s sds
 * @return 空闲长度
 */
size_t sdsavail(const sds s);

/**
 * 用空字符将 sds 扩展至指定长度 len
 *
 * @param s sds
 * @param len 长度
 * @return 扩展后的 sds
 */
sds sdsgrowzero(sds s, size_t len);

/**
 * 将长度为 len 的字符串 t 追加到指定 sds 的末尾
 *
 * @param s 原 sds
 * @param t 要追加的字符串
 * @param len 要追加的字符串
 * @return 追加后的 sds
 */
sds sdscatlen(sds s, const void *t, size_t len);

/**
 * 将字符串 t 追加到指定 sds 的末尾
 *
 * @param s 原 sds
 * @param t 要追加的字符串
 * @return 追加后的 sds
 */
sds sdscat(sds s, const char *t);

/**
 * 将一个 sds 追加到另一个 sds 的末尾
 *
 * @param s 原 sds
 * @param t 要追加的 sds
 * @return 追加后的 sds
 */
sds sdscatsds(sds s, const sds t);

/**
 * 将指定字符串 t 的前 len 个字符复制到 sds 中
 *
 * @param s 原 sds
 * @param t 要复制到 sds 的字符串
 * @param len 长度
 * @return 复制后的 sds
 */
sds sdscpylen(sds s, const char *t, size_t len);

/**
 * 将指定的字符串 t 复制到 sds 中
 *
 * @param s sds
 * @param t 要复制到 sds 的字符串
 * @return 复制后的 sds
 */
sds sdscpy(sds s, const char *t);

/**
 * 接受一个 printf 格式的字符串, 将它拼接到 sds 的末尾
 *
 * @param s sds
 * @param fmt printf 格式的字符串
 * @param ap 参数列表
 * @return 拼接后的 sds
 */
sds sdscatvprintf(sds s, const char *fmt, va_list ap);


// 如果设置了 gcc, 则需要对可变参数进行检查
#ifdef __GNUC__

/**
 * 接受一个 printf 格式的字符串, 将它拼接到 sds 的末尾
 *
 * @param s sds
 * @param fmt printf 格式的字符串
 * @param ... 参数列表
 * @return 拼接后的 sds
 */
sds sdscatprintf(sds s, const char *fmt, ...)

/**
 * 配置对 printf 进行检查, 2 是指 printf 格式字符串在第二个参数的位置,
 * 3 是指 printf 格式字符串对应的参数从第三个参数位置开始
 */
__attribute__((format(printf, 2, 3)));

#else

/**
 * 接受一个 printf 格式的字符串, 将它拼接到 sds 的末尾
 *
 * @param s sds
 * @param fmt printf 格式的字符串
 * @param ... 参数列表
 * @return 拼接后的 sds
 */
sds sdscatprintf(sds s, const char *fmt, ...);

#endif

/**
 * 与 sdscatprintf 函数是类似的, 但是性能更好
 *
 * @param s sds
 * @param fmt printf 格式的字符串
 * @param ... 参数列表
 * @return 拼接后的 sds
 */
sds sdscatfmt(sds s, char const *fmt, ...);

/**
 * 在 sds 中从左右两端清除 cset 中指定的字符
 *
 * @param s sds
 * @param cset 要清除的字符集合
 * @return 清除字符后的 sds
 */
sds sdstrim(sds s, const char *cset);

/**
 * 截取 [start, end] 部分的子字符串
 *
 * @param s sds
 * @param start 开始位置
 * @param end 结束位置
 */
void sdsrange(sds s, int start, int end);

/**
 * 应该是被遗弃的函数, 更新 sdshdr 中的 len 和 free
 *
 * @param s sds
 */
void sdsupdatelen(sds s);

/**
 * 重置 sds
 *
 * @param s sds
 */
void sdsclear(sds s);

/**
 * 比较两个 sds
 *
 * @param s1 要比较的第一个 sds
 * @param s2 要比较的第二个 sds
 * @return 1: s1 > s2; -1: s1 < s2; 0: s1 == s2
 */
int sdscmp(const sds s1, const sds s2);

/**
 * 使用分割符 sep 对 sds 进行分割并返回一个 sds 数组, 参数中的 count 将保存
 *
 * @param s sds
 * @param len sds 的长度
 * @param sep 分割符
 * @param seplen 分割符的长度
 * @param count 返回值数组的长度
 * @return 分割后的 sds 数组
 */
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);

/**
 * 将 sdssplitlen 函数的返回值清空
 *
 * @param tokens sds 数组
 * @param count 数组的长度
 */
void sdsfreesplitres(sds *tokens, int count);

/**
 * 将 sds 中所有的字符转为小写
 *
 * @param s sds
 */
void sdstolower(sds s);

/**
 * 应该是废弃的函数, 将 sds 中所有的字符转为大写
 *
 * @param s sds
 */
void sdstoupper(sds s);

/**
 * 根据给定的一个 long long 类型的值, 创建一个 sds
 *
 * @param value 一个 long long 类型的值
 * @return 创建好的 sds
 */
sds sdsfromlonglong(long long value);

/**
 * 在 sds 中追加保存一个引用字符串
 *
 * @param s sds
 * @param p 引用字符串
 * @param len 引用字符串的长度
 * @return 追加操作执行完成后的 sds
 */
sds sdscatrepr(sds s, const char *p, size_t len);

/**
 * 将一行文本分割为多个参数, 参数的个数由变量 argc 保存
 *
 * @param line 一行文本
 * @param argc 返回的数组的长度
 * @return 分割后的 sds 数组
 */
sds *sdssplitargs(const char *line, int *argc);

/**
 * 将 sds 中, from 字符串中出现的字符替换为 to 字符串中的字符
 *
 * @param s sds
 * @param from 源
 * @param to 目标
 * @param setlen 字符串长度
 * @return 替换后的 sds
 */
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);

/**
 * 使用指定的分割符 sep, 将字符串数组 argc 添加到 sds 中
 *
 * @param argv 字符串数组
 * @param argc 数组长度
 * @param sep 分割符
 * @return 添加后的 sds
 */
sds sdsjoin(char **argv, int argc, char *sep);

/**
 * sds 扩容
 *
 * @param s sds
 * @param addlen 要追加的字节长度
 * @return 扩容后的 sds
 */
sds sdsMakeRoomFor(sds s, size_t addlen);

/**
 * 根据长度 incr, 增加 sds 长度, 减少 free 长度
 *
 * @param s sds
 * @param incr 指定的长度
 */
void sdsIncrLen(sds s, int incr);

/**
 * 回收 sds 中的空闲区域
 *
 * @param s sds
 * @return 回收后的 sds
 */
sds sdsRemoveFreeSpace(sds s);

/**
 * 返回给定 sds 分配的内存字节数
 *
 * @param s sds
 * @return 占用内存大小
 */
size_t sdsAllocSize(sds s);

#endif
