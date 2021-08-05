/* SDSLib, A C dynamic strings library
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "sds.h"
#include "zmalloc.h"

/**
 * 根据给定的初始字符串 init 以及初始长度 initlen 创建一个新的 sds. 如果 init 为空,
 * 那么 sds 中将不保存任何内容. 使用方法为:
 * @code sds mystring = sdsnewlen("abc", 3); @endcode
 * 需要注意的是:
 * - 在 buf 数组的末尾有一个隐含的字符 '\0'
 * - 因为长度 len 保存在 sdshdr 中, 所以就算 sds 中间保存了 '\0', 也依然可以保证二
 *   进制安全
 *
 * @param init 初始字符串
 * @param initlen 初始长度
 * @return 创建后的字符串
 */
sds sdsnewlen(const void *init, size_t initlen) {
    struct sdshdr *sh;

    // 判断初始字符串是否存在
    if (init) {
        // 不初始化所分配的内存
        sh = zmalloc(sizeof(struct sdshdr) + initlen + 1);
    } else {
        // 将分配的内存全部初始化为 0
        sh = zcalloc(sizeof(struct sdshdr) + initlen + 1);
    }

    // 内存分配失败则直接返回
    if (sh == NULL) {
        return NULL;
    }

    // 设置初始长度
    sh->len = initlen;
    // 设置空闲长度
    sh->free = 0;

    // 如果制定了初始值, 则把初始化复制到 buf 数组中
    if (initlen && init) {
        memcpy(sh->buf, init, initlen);
    }

    // 设置 buf 以 '\0' 结尾
    sh->buf[initlen] = '\0';

    // 返回 buf 数组
    return (char *) sh->buf;
}

/**
 * 创建一个空的 sds, 但是最后会有一个 '\0'.
 *
 * @return 一个空的 sds
 */
sds sdsempty(void) {
    return sdsnewlen("", 0);
}

/**
 * 根据指定的字符串创建一个 sds.
 *
 * @param init 初始字符串
 * @return 创建好的 sds
 */
sds sdsnew(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/**
 * 将给定的 sds 复制, 得到一个新的 sds.
 *
 * @param s sds
 * @return 复制后得到的 sds
 */
sds sdsdup(const sds s) {
    return sdsnewlen(s, sdslen(s));
}

/**
 * 释放指定的 sds.
 *
 * @param s sds
 */
void sdsfree(sds s) {
    if (s == NULL) {
        return;
    }
    zfree(s - sizeof(struct sdshdr));
}

/**
 * 将 sdshdr 中保存的已使用长度更新为 strlen 函数返回的值, 这个方法在手动修改 buf 数组
 * 时十分有效, 比如:
 * @code
 * sds s = sdsnew("foobar");
 * s[2] = '\0';
 * sdsupdatelen(s);
 * printf("%d\n", sdslen(s));
 * @endcode
 * 在未更新时, 输出结果将会是 6, 而更新后将输出 2.
 *
 * 注意:
 * - 这个方法应该已经废弃了
 * - 调用 strlen 将忽略第一个 '\0' 后的所有字符
 *
 * @param s sds
 */
void sdsupdatelen(sds s) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    // 获取 s 的长度
    int reallen = strlen(s);
    // 空闲长度需要加上差值
    sh->free += (sh->len - reallen);
    // 更新已使用长度
    sh->len = reallen;
}

/**
 * 清空 sds, 此处的清空只是指将 sds 标记为未使用状态, 即整个 sds 都是未使用空间, sds
 * 的已使用空间为 0, 同时将 buf 数组的首个位置标记为 '\0'. 但是已申请的内存是不会被释
 * 放的, 这样可以避免重复申请内存导致的开销.
 *
 * @param s sds
 */
void sdsclear(sds s) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    sh->free += sh->len;
    sh->len = 0;
    sh->buf[0] = '\0';
}

/**
 * 对 sds 扩容, 保证在调用这个函数后至少有 addlen + 1 个可用空间.<br>
 * 注意:
 * - 多一个 1 是因为结尾要保存 '\0'
 * - 这个方法不会修改 len, 只是修改 free
 *
 * @param s sds
 * @param addlen 要添加的长度
 * @return 扩容后的 sds
 */
sds sdsMakeRoomFor(sds s, size_t addlen) {
    // 如果空闲空间比指定的长度要大, 那就没必要扩容, 直接返回
    if (sdsavail(s) >= addlen) {
        return s;
    }

    // 当前 sds 的长度
    size_t len = sdslen(s);
    // 新的 sds 长度
    size_t newlen = len + addlen;

    // 小于 1M 时, 每次将空间扩大为当前的 2 倍
    // 大于等于 1M 时, 每次增加 1M 的空间
    if (newlen < SDS_MAX_PREALLOC) {
        newlen *= 2;
    } else {
        newlen += SDS_MAX_PREALLOC;
    }

    // 原始头信息
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    // 重新分配内存
    struct sdshdr *newsh = zrealloc(sh, sizeof(struct sdshdr) + newlen + 1);
    // 如果分配失败则返回 NULL
    if (newsh == NULL) {
        return NULL;
    }

    // 更新新的头信息中的空闲长度
    newsh->free = newlen - len;
    // 返回 buf 数组
    return newsh->buf;
}

/**
 * 回收 sds 中的空闲区域, 并将 free 置为 0, 在执行这个操作后如果再修改 sds 就需要重新分
 * 配内存.
 *
 * @param s sds
 * @return 回收空闲区域后的 sds
 */
sds sdsRemoveFreeSpace(sds s) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    sh = zrealloc(sh, sizeof(struct sdshdr) + sh->len + 1);
    sh->free = 0;
    return sh->buf;
}

/**
 * 返回 sds 占用的内存大小, 包括:
 * - 头信息
 * - 已使用空间
 * - 未使用空间
 * - 结尾的 '\0'
 *
 * @param s sds
 * @return 分配的内存大小
 */
size_t sdsAllocSize(sds s) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    return sizeof(*sh) + sh->len + sh->free + 1;
}

/**
 * 增加 sds 的长度并减少空闲长度并减少空闲长度. 新的 sds 的末尾会有一个 '\0'.
 * 一般来说首先会调用 sdsMakeRoomFor 函数进行扩容, 扩容后会有一些写操作, 在
 * 这些操作通过这个函数可以修正因为写操作导致的长度不一致. 看一下下面的例子,
 * 这个例子是直接操作内存将字符串放到了 buf 数组的后面, 所以在操作结束后需要修
 * 正长度:
 * @code
 * oldlen = sdslen(s);
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sdsIncrLen(s, nread);
 * @endcode
 * 注意:
 * - incr 可以为负数, 如果是负数将减少 sds 的长度
 *
 * @param s sds
 * @param incr 要增加/减少的长度
 */
void sdsIncrLen(sds s, int incr) {
    // 当前 sds 的头信息
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));

    // 如果长度大于等于 0, 证明是增加长度, 那么需要比较 free
    // 如果小于 0, 证明是减少长度, 那么需要比较 len
    if (incr >= 0) {
        assert(sh->free >= (unsigned int) incr);
    } else {
        assert(sh->len >= (unsigned int) (-incr));
    }

    // 增加/减少长度
    sh->len += incr;
    // 增加/减少空闲长度
    sh->free -= incr;
    // 最后一位置位 \0
    s[sh->len] = '\0';
}

/**
 * 将 sds 扩展到指定的长度, 新分配的内存将设置为 0. 如果指定的长度比当前长度小, 那么
 * 什么操作也不会进行.
 *
 * @param s sds
 * @param len 长度
 * @return 增加后的 sds
 */
sds sdsgrowzero(sds s, size_t len) {
    // 当前的头信息
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    // 当前已使用长度
    size_t curlen = sh->len;
    // 如果指定的长度比当前长度要小, 那就没必要扩容
    if (len <= curlen) {
        return s;
    }
    // 扩容
    s = sdsMakeRoomFor(s, len - curlen);
    if (s == NULL) {
        return NULL;
    }

    // 重新计算指针
    sh = (void *) (s - (sizeof(struct sdshdr)));
    // 设置为 0
    memset(s + curlen, 0, (len - curlen + 1));
    // 此时计算的总长度是不包含后面的 0 的
    size_t totlen = sh->len + sh->free;
    // 更新已使用长度
    sh->len = len;
    // 更新空闲长度
    sh->free = totlen - sh->len;
    return s;
}

/**
 * 将指定的二进制安全的字符串的前 len 字节追加到 sds 的末尾.
 *
 * @param s sds
 * @param t 一个二进制安全的字符串
 * @param len 指定的长度
 * @return 追加后的字符串
 */
sds sdscatlen(sds s, const void *t, size_t len) {
    // 扩容
    s = sdsMakeRoomFor(s, len);
    if (s == NULL) {
        return NULL;
    }

    // 头信息
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    // 当前长度
    size_t curlen = sdslen(s);
    // 复制字符串
    memcpy(s + curlen, t, len);
    // 更新已使用长度
    sh->len = curlen + len;
    // 更新空闲长度
    sh->free = sh->free - len;
    // 设置末尾的 \0
    s[curlen + len] = '\0';
    return s;
}

/**
 * 将指定的字符串追加到 sds 的末尾.
 *
 * @param s sds
 * @param t 要追加的字符串
 * @return 追加后的 sds
 */
sds sdscat(sds s, const char *t) {
    return sdscatlen(s, t, strlen(t));
}

/**
 * 将一个指定 sds 追加到另一个 sds 的末尾.
 *
 * @param s 被追加的 sds
 * @param t 一个指定要追加的 sds
 * @return 追加后的 sds
 */
sds sdscatsds(sds s, const sds t) {
    return sdscatlen(s, t, sdslen(t));
}

/**
 * 将指定的字符串的前 len 字节复制到 sds 中.
 *
 * @param s sds
 * @param t 指定的字符串
 * @param len 长度
 * @return 复制后的字符串
 */
sds sdscpylen(sds s, const char *t, size_t len) {
    // 头信息
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    // 总长度
    size_t totlen = sh->free + sh->len;
    // 如果长度不够需要先扩容
    if (totlen < len) {
        s = sdsMakeRoomFor(s, len - sh->len);
        if (s == NULL) {
            return NULL;
        }
        // 扩容后重新计算指针与总长度
        sh = (void *) (s - (sizeof(struct sdshdr)));
        totlen = sh->free + sh->len;
    }
    // 复制字符串
    memcpy(s, t, len);
    // 填充头信息
    s[len] = '\0';
    sh->len = len;
    sh->free = totlen - len;
    return s;
}

/**
 * 将给定的字符串复制到 sds 中.
 *
 * @param s sds
 * @param t 指定的字符串
 * @return 复制后的 sds
 */
sds sdscpy(sds s, const char *t) {
    return sdscpylen(s, t, strlen(t));
}

/**
 * 帮助实现数字到字符串的转换, s 的长度应该至少有 21 字节.
 */
#define SDS_LLSTR_SIZE 21

/**
 * 将指定的 long long 类型的 value 转换为字符串并返回这
 * 个字符串的长度(不包含结尾的 \0).
 *
 * @param s 字符串指针
 * @param value 要转换的数字
 * @return 转换后字符串的长度
 */
int sdsll2str(char *s, long long value) {
    // 先统一设置为正数
    unsigned long long v = (value < 0) ? -value : value;

    // 逆序添加数字的每一位
    char *p = s;
    do {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);

    // 如果是负数则加上负号
    if (value < 0) {
        *p++ = '-';
    }

    // 计算长度并设置 \0
    size_t l = p - s;
    *p = '\0';

    // 通过两个指针交换字符的方式来反转字符串
    char *aux;
    p--;
    while (s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }

    // 返回字符串的长度
    return l;
}

/**
 * 将指定的 unsigned long long 类型的 v 转换为字符串并返回这个字符串的
 * 长度(不包含结尾的 \0). 与上一个函数的区别是 v 的类型.
 *
 * @param s 字符串指针
 * @param value 要转换的数字
 * @return 转换后字符串的长度
 */
int sdsull2str(char *s, unsigned long long v) {
    // 逆序保存数字
    char *p = s;
    do {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);

    // 计算长度并设置 \0
    size_t l = p - s;
    *p = '\0';

    // 通过两个指针反转字符串
    char *aux;
    p--;
    while (s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }

    // 返回长度
    return l;
}

/**
 * 根据指定的 long long 类型的 value 创建一个新的 sds. 这个函数比
 * sdscatprintf() 函数要更快.
 *
 * @param value 要转换的数字
 * @return 创建好的 sds
 */
sds sdsfromlonglong(long long value) {
    char buf[SDS_LLSTR_SIZE];
    int len = sdsll2str(buf, value);

    return sdsnewlen(buf, len);
}

/**
 * 根据指定的格式 fmt 在已有 sds 后面拼接并生成新的 sds.
 *
 * @param s 已有的 sds
 * @param fmt 一个格式
 * @param ap 参数列表
 * @return 拼接后的 sds
 */
sds sdscatvprintf(sds s, const char *fmt, va_list ap) {
    // 在栈上建立一个长度是 1024 的 buffer
    char staticbuf[1024];
    char *buf = staticbuf;
    char *t;

    // 估算一下可能的长度
    // 假设是 2 倍
    size_t buflen = strlen(fmt) * 2;

    // 如果栈上分配的长度不够, 那么在堆上重新分配内存
    // 如果长度够则指定长度是 1024
    if (buflen > sizeof(staticbuf)) {
        buf = zmalloc(buflen);
        if (buf == NULL) {
            return NULL;
        }
    } else {
        buflen = sizeof(staticbuf);
    }

    // 在 buf 中保存格式化字符串
    va_list cpy;
    while (1) {
        // 在末尾标记一个 \0
        buf[buflen - 2] = '\0';

        // 根据给定的格式 fmt 对 cpy 进行格式化
        // 将前 buflen 长度的内容写入 buf 中
        va_copy(cpy, ap);
        vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);

        // 如果提前标记的 \0 没有了
        // 也就是说长度不够
        // 那么将长度扩大两倍并重新分配内存
        if (buf[buflen - 2] != '\0') {
            if (buf != staticbuf) {
                zfree(buf);
            }
            buflen *= 2;
            buf = zmalloc(buflen);
            if (buf == NULL) {
                return NULL;
            }
            continue;
        }

        // 当写入成功时退出循环
        break;
    }

    // 将 buf 拼接到 sds 的末尾并返回
    t = sdscat(s, buf);
    if (buf != staticbuf) {
        zfree(buf);
    }
    return t;
}

/**
 * 将一个 printf 格式的字符串拼接到 sds 的末尾. 在调用这个函数后, 原指针将变为无效的,
 * 需要将引用指向返回值. 举个例子:
 * @code
 * s = sdsnew("Sum is: ");
 * s = sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 * @endcode
 * 但有时候仅仅是希望通过这个函数来创建一个 printf 格式的 sds, 而不是在已有的 sds 的
 * 末尾进行拼接. 对于这种情况, 可以直接传入 sdsempty(), 比如:
 * @code
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 * @endcode
 *
 * @param s sds
 * @param fmt 一个格式
 * @param ... 参数列表
 * @return 拼接后的 sds
 */
sds sdscatprintf(sds s, const char *fmt, ...) {
    // 保存结果
    char *t;

    // 将参数列表通过 va_list 保存并调用 sdscatvprintf()
    // 函数进行拼接
    va_list ap;
    va_start(ap, fmt);
    t = sdscatvprintf(s, fmt, ap);
    va_end(ap);

    return t;
}

/**
 * 这个函数与上面的 sdscatprintf() 函数类似, 由于是直接操作 sds 所以它的处理速度
 * 更快, 因为不再依赖于 sprintf() 相关的函数(这些函数通常都很慢). 但是这个函数只能
 * 处理以下格式符:
 * - %s: C 字符串
 * - %S: SDS 字符串
 * - %i: 有符号整型
 * - %I: 有符号 64 位整型(long long, uint64_t)
 * - %u: 无符号 整型
 * - %U: 无符号 64 位整型(unsigned long long, uint64_t)
 * - %%: 符号 %
 * @param s
 * @param fmt
 * @param ...
 * @return 按照指定格式拼接生成的 sds
 */
sds sdscatfmt(sds s, char const *fmt, ...) {
    // 头信息
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    // 初始长度
    size_t initlen = sdslen(s);
    // 指向 fmt 的指针
    const char *f = fmt;
    // 可变参数列表
    va_list ap;

    va_start(ap, fmt);
    // 下一个需要处理的格式符
    f = fmt;
    // 字符的起始位置
    int i = initlen;

    // 遍历 fmt
    while (*f) {
        // 确保至少有一个字节的可用空间
        if (sh->free == 0) {
            s = sdsMakeRoomFor(s, 1);
            sh = (void *) (s - (sizeof(struct sdshdr)));
        }

        // 保存 % 后面的那个字符
        char next;
        // 可变参数 -> 字符串
        char *str;
        // 字符串的长度
        unsigned int l;
        // 可变参数 -> long long
        long long num;
        // 可变参数 -> unsigned long long
        unsigned long long unum;

        // 判断是否是 % 开头
        switch (*f) {
            case '%':
                // % 的下一个字符
                next = *(f + 1);
                f++;

                // 判断类型
                switch (next) {
                    case 's':
                    case 'S':
                        // 当前的字符串
                        str = va_arg(ap, char*);

                        // 根据是 s 还是 S, 决定是使用 strlen 还是 sdslen
                        l = (next == 's') ? strlen(str) : sdslen(str);

                        // 如果空间不够需要扩容
                        if (sh->free < l) {
                            s = sdsMakeRoomFor(s, l);
                            sh = (void *) (s - (sizeof(struct sdshdr)));
                        }

                        // 复制到 sds 中
                        memcpy(s + i, str, l);

                        // 设置已使用长度和空闲长度
                        sh->len += l;
                        sh->free -= l;

                        // 移动起始位置
                        i += l;

                        break;
                    case 'i':
                    case 'I':
                        // 根据是 i 还是 I, 决定是 int 还是 long long
                        if (next == 'i') {
                            num = va_arg(ap, int);
                        } else {
                            num = va_arg(ap, long long);
                        }

                        // 不用花括号会提示变量重复定义
                        {
                            // 通过 sdsll2str 将数字进行转换, 保存到 buf 数组中
                            char buf[SDS_LLSTR_SIZE];
                            l = sdsll2str(buf, num);

                            // 如果长度不够, 需要先扩容
                            if (sh->free < l) {
                                s = sdsMakeRoomFor(s, l);
                                sh = (void *) (s - (sizeof(struct sdshdr)));
                            }

                            // 复制到 sds 中
                            memcpy(s + i, buf, l);

                            // 设置已使用长度和空闲长度
                            sh->len += l;
                            sh->free -= l;

                            // 移动指针
                            i += l;
                        }

                        break;
                    case 'u':
                    case 'U':
                        // 根据是 u 还是 U, 决定是 unsigned int 还是 unsigned long long
                        if (next == 'u') {
                            unum = va_arg(ap, unsigned int);
                        } else {
                            unum = va_arg(ap, unsigned long long);
                        }

                        {
                            // 通过 sdsull2str 对数字进行转换并保存到 buf 中
                            char buf[SDS_LLSTR_SIZE];
                            l = sdsull2str(buf, unum);

                            // 如果长度不够, 需要先扩容
                            if (sh->free < l) {
                                s = sdsMakeRoomFor(s, l);
                                sh = (void *) (s - (sizeof(struct sdshdr)));
                            }

                            // 复制到 sds 中
                            memcpy(s + i, buf, l);

                            // 设置已使用长度和空闲长度
                            sh->len += l;
                            sh->free -= l;

                            // 移动指针
                            i += l;
                        }
                        break;
                    default:
                        // 处理 %% 或者是其他不支持字符的情况
                        s[i++] = next;
                        sh->len += 1;
                        sh->free -= 1;
                        break;
                }
                break;
            default:
                // 不是 % 就继续往后走
                s[i++] = *f;
                sh->len += 1;
                sh->free -= 1;
                break;
        }
        f++;
    }
    va_end(ap);

    // 添加 \0
    s[i] = '\0';
    return s;
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 * s = sdstrim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "Hello World".
 */
sds sdstrim(sds s, const char *cset) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s + sdslen(s) - 1;
    while (sp <= end && strchr(cset, *sp)) sp++;
    while (ep > start && strchr(cset, *ep)) ep--;
    len = (sp > ep) ? 0 : ((ep - sp) + 1);
    if (sh->buf != sp) memmove(sh->buf, sp, len);
    sh->buf[len] = '\0';
    sh->free = sh->free + (sh->len - len);
    sh->len = len;
    return s;
}

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = sdsnew("Hello World");
 * sdsrange(s,1,-1); => "ello World"
 */
void sdsrange(sds s, int start, int end) {
    struct sdshdr *sh = (void *) (s - (sizeof(struct sdshdr)));
    size_t newlen, len = sdslen(s);

    if (len == 0) return;
    if (start < 0) {
        start = len + start;
        if (start < 0) start = 0;
    }
    if (end < 0) {
        end = len + end;
        if (end < 0) end = 0;
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
    if (newlen != 0) {
        if (start >= (signed) len) {
            newlen = 0;
        } else if (end >= (signed) len) {
            end = len - 1;
            newlen = (start > end) ? 0 : (end - start) + 1;
        }
    } else {
        start = 0;
    }
    if (start && newlen) memmove(sh->buf, sh->buf + start, newlen);
    sh->buf[newlen] = 0;
    sh->free = sh->free + (sh->len - newlen);
    sh->len = newlen;
}

/* Apply tolower() to every character of the sds string 's'. */
void sdstolower(sds s) {
    int len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}

/* Apply toupper() to every character of the sds string 's'. */
void sdstoupper(sds s) {
    int len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}

/* Compare two sds strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
int sdscmp(const sds s1, const sds s2) {
    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);
    if (cmp == 0) return l1 - l2;
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count) {
    int elements = 0, slots = 5, start = 0, j;
    sds *tokens;

    if (seplen < 1 || len < 0) return NULL;

    tokens = zmalloc(sizeof(sds) * slots);
    if (tokens == NULL) return NULL;

    if (len == 0) {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len - (seplen - 1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements + 2) {
            sds *newtokens;

            slots *= 2;
            newtokens = zrealloc(tokens, sizeof(sds) * slots);
            if (newtokens == NULL) goto cleanup;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s + j) == sep[0]) || (memcmp(s + j, sep, seplen) == 0)) {
            tokens[elements] = sdsnewlen(s + start, j - start);
            if (tokens[elements] == NULL) goto cleanup;
            elements++;
            start = j + seplen;
            j = j + seplen - 1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sdsnewlen(s + start, len - start);
    if (tokens[elements] == NULL) goto cleanup;
    elements++;
    *count = elements;
    return tokens;

    cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) sdsfree(tokens[i]);
        zfree(tokens);
        *count = 0;
        return NULL;
    }
}

/* Free the result returned by sdssplitlen(), or do nothing if 'tokens' is NULL. */
void sdsfreesplitres(sds *tokens, int count) {
    if (!tokens) return;
    while (count--)
        sdsfree(tokens[count]);
    zfree(tokens);
}

/* Append to the sds string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
sds sdscatrepr(sds s, const char *p, size_t len) {
    s = sdscatlen(s, "\"", 1);
    while (len--) {
        switch (*p) {
            case '\\':
            case '"':
                s = sdscatprintf(s, "\\%c", *p);
                break;
            case '\n':
                s = sdscatlen(s, "\\n", 2);
                break;
            case '\r':
                s = sdscatlen(s, "\\r", 2);
                break;
            case '\t':
                s = sdscatlen(s, "\\t", 2);
                break;
            case '\a':
                s = sdscatlen(s, "\\a", 2);
                break;
            case '\b':
                s = sdscatlen(s, "\\b", 2);
                break;
            default:
                if (isprint(*p))
                    s = sdscatprintf(s, "%c", *p);
                else
                    s = sdscatprintf(s, "\\x%02x", (unsigned char) *p);
                break;
        }
        p++;
    }
    return sdscatlen(s, "\"", 1);
}

/* Helper function for sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit. */
int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for sdssplitargs() that converts a hex digit into an
 * integer from 0 to 15 */
int hex_digit_to_int(char c) {
    switch (c) {
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        case 'a':
        case 'A':
            return 10;
        case 'b':
        case 'B':
            return 11;
        case 'c':
        case 'C':
            return 12;
        case 'd':
        case 'D':
            return 13;
        case 'e':
        case 'E':
            return 14;
        case 'f':
        case 'F':
            return 15;
        default:
            return 0;
    }
}

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of sds is returned.
 *
 * The caller should free the resulting array of sds strings with
 * sdsfreesplitres().
 *
 * Note that sdscatrepr() is able to convert back a string into
 * a quoted string in the same format sdssplitargs() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
sds *sdssplitargs(const char *line, int *argc) {
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while (1) {
        /* skip blanks */
        while (*p && isspace(*p)) p++;
        if (*p) {
            /* get a token */
            int inq = 0;  /* set to 1 if we are in "quotes" */
            int insq = 0; /* set to 1 if we are in 'single quotes' */
            int done = 0;

            if (current == NULL) current = sdsempty();
            while (!done) {
                if (inq) {
                    if (*p == '\\' && *(p + 1) == 'x' &&
                        is_hex_digit(*(p + 2)) &&
                        is_hex_digit(*(p + 3))) {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p + 2)) * 16) +
                               hex_digit_to_int(*(p + 3));
                        current = sdscatlen(current, (char *) &byte, 1);
                        p += 3;
                    } else if (*p == '\\' && *(p + 1)) {
                        char c;

                        p++;
                        switch (*p) {
                            case 'n':
                                c = '\n';
                                break;
                            case 'r':
                                c = '\r';
                                break;
                            case 't':
                                c = '\t';
                                break;
                            case 'b':
                                c = '\b';
                                break;
                            case 'a':
                                c = '\a';
                                break;
                            default:
                                c = *p;
                                break;
                        }
                        current = sdscatlen(current, &c, 1);
                    } else if (*p == '"') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1))) goto err;
                        done = 1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdscatlen(current, p, 1);
                    }
                } else if (insq) {
                    if (*p == '\\' && *(p + 1) == '\'') {
                        p++;
                        current = sdscatlen(current, "'", 1);
                    } else if (*p == '\'') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1))) goto err;
                        done = 1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdscatlen(current, p, 1);
                    }
                } else {
                    switch (*p) {
                        case ' ':
                        case '\n':
                        case '\r':
                        case '\t':
                        case '\0':
                            done = 1;
                            break;
                        case '"':
                            inq = 1;
                            break;
                        case '\'':
                            insq = 1;
                            break;
                        default:
                            current = sdscatlen(current, p, 1);
                            break;
                    }
                }
                if (*p) p++;
            }
            /* add the token to the vector */
            vector = zrealloc(vector, ((*argc) + 1) * sizeof(char *));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        } else {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL) vector = zmalloc(sizeof(void *));
            return vector;
        }
    }

    err:
    while ((*argc)--)
        sdsfree(vector[*argc]);
    zfree(vector);
    if (current) sdsfree(current);
    *argc = 0;
    return NULL;
}

/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: sdsmapchars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the sds string pointer, that is always the same
 * as the input pointer since no resize is needed. */
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen) {
    size_t j, i, l = sdslen(s);

    for (j = 0; j < l; j++) {
        for (i = 0; i < setlen; i++) {
            if (s[j] == from[i]) {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an sds string. */
sds sdsjoin(char **argv, int argc, char *sep) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscat(join, argv[j]);
        if (j != argc - 1) join = sdscat(join, sep);
    }
    return join;
}

#ifdef SDS_TEST_MAIN
#include <stdio.h>
#include "testhelp.h"
#include "limits.h"

int main(void) {
    {
        struct sdshdr *sh;
        sds x = sdsnew("foo"), y;

        test_cond("Create a string and obtain the length",
            sdslen(x) == 3 && memcmp(x,"foo\0",4) == 0)

        sdsfree(x);
        x = sdsnewlen("foo",2);
        test_cond("Create a string with specified length",
            sdslen(x) == 2 && memcmp(x,"fo\0",3) == 0)

        x = sdscat(x,"bar");
        test_cond("Strings concatenation",
            sdslen(x) == 5 && memcmp(x,"fobar\0",6) == 0);

        x = sdscpy(x,"a");
        test_cond("sdscpy() against an originally longer string",
            sdslen(x) == 1 && memcmp(x,"a\0",2) == 0)

        x = sdscpy(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
        test_cond("sdscpy() against an originally shorter string",
            sdslen(x) == 33 &&
            memcmp(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0",33) == 0)

        sdsfree(x);
        x = sdscatprintf(sdsempty(),"%d",123);
        test_cond("sdscatprintf() seems working in the base case",
            sdslen(x) == 3 && memcmp(x,"123\0",4) == 0)

        sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "Hello %s World %I,%I--", "Hi!", LLONG_MIN,LLONG_MAX);
        test_cond("sdscatfmt() seems working in the base case",
            sdslen(x) == 60 &&
            memcmp(x,"--Hello Hi! World -9223372036854775808,"
                     "9223372036854775807--",60) == 0)

        sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
        test_cond("sdscatfmt() seems working with unsigned numbers",
            sdslen(x) == 35 &&
            memcmp(x,"--4294967295,18446744073709551615--",35) == 0)

        sdsfree(x);
        x = sdsnew("xxciaoyyy");
        sdstrim(x,"xy");
        test_cond("sdstrim() correctly trims characters",
            sdslen(x) == 4 && memcmp(x,"ciao\0",5) == 0)

        y = sdsdup(x);
        sdsrange(y,1,1);
        test_cond("sdsrange(...,1,1)",
            sdslen(y) == 1 && memcmp(y,"i\0",2) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,1,-1);
        test_cond("sdsrange(...,1,-1)",
            sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,-2,-1);
        test_cond("sdsrange(...,-2,-1)",
            sdslen(y) == 2 && memcmp(y,"ao\0",3) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,2,1);
        test_cond("sdsrange(...,2,1)",
            sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,1,100);
        test_cond("sdsrange(...,1,100)",
            sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,100,100);
        test_cond("sdsrange(...,100,100)",
            sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("foo");
        y = sdsnew("foa");
        test_cond("sdscmp(foo,foa)", sdscmp(x,y) > 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("bar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x,y) == 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("aar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x,y) < 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnewlen("\a\n\0foo\r",7);
        y = sdscatrepr(sdsempty(),x,sdslen(x));
        test_cond("sdscatrepr(...data...)",
            memcmp(y,"\"\\a\\n\\x00foo\\r\"",15) == 0)

        {
            int oldfree;

            sdsfree(x);
            sdsfree(y);
            x = sdsnew("0");
            sh = (void*) (x-(sizeof(struct sdshdr)));
            test_cond("sdsnew() free/len buffers", sh->len == 1 && sh->free == 0);
            x = sdsMakeRoomFor(x,1);
            sh = (void*) (x-(sizeof(struct sdshdr)));
            test_cond("sdsMakeRoomFor()", sh->len == 1 && sh->free > 0);
            oldfree = sh->free;
            x[1] = '1';
            sdsIncrLen(x,1);
            test_cond("sdsIncrLen() -- content", x[0] == '0' && x[1] == '1');
            test_cond("sdsIncrLen() -- len", sh->len == 2);
            test_cond("sdsIncrLen() -- free", sh->free == oldfree-1);

            sdsfree(x);
        }
    }
    test_report()
    return 0;
}
#endif
