/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
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

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

/**
 * 用于返回值, 表示处理成功
 */
#define DICT_OK 0

/**
 * 用于返回值, 表示处理失败
 */
#define DICT_ERR 1

/**
 * 避免编译器 warning
 */
#define DICT_NOTUSED(V) ((void) V)

/**
 * 每个键值对
 */
typedef struct dictEntry {
    /**
     * 键
     */
    void *key;

    /**
     * 值, 类型只能是下面列出来的几种
     */
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;

    /**
     * 指向下个节点, 形成链表
     */
    struct dictEntry *next;
} dictEntry;

/**
 * 一组用于操作数据的函数集
 */
typedef struct dictType {
    /**
     * 计算哈希值
     *
     * @param key 键
     * @return 哈希值
     */
    unsigned int (*hashFunction)(const void *key);

    /**
     * 复制键
     *
     * @param privdata 可选参数
     * @param key 键
     * @return 复制后的键
     */
    void *(*keyDup)(void *privdata, const void *key);

    /**
     * 复制值
     *
     * @param privdata 可选参数
     * @param obj 值
     * @return 复制后的值
     */
    void *(*valDup)(void *privdata, const void *obj);

    /**
     * 对比键
     *
     * @param privdata 可选参数
     * @param key1 要比较的第一个键
     * @param key2 要比较的第二个键
     * @return 比较结果
     */
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);

    /**
     * 销毁键
     *
     * @param privdata 可选参数
     * @param key 键
     */
    void (*keyDestructor)(void *privdata, void *key);

    /**
     * 销毁值
     *
     * @param privdata 可选参数
     * @param obj 值
     */
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/**
 * dicth(ash)t(able), 哈希表
 */
typedef struct dictht {
    /**
     * 键值对数组, 同索引位置的会构成链表
     */
    dictEntry **table;

    /**
     * 哈希表的大小
     */
    unsigned long size;

    /**
     * 掩码, 这个值等于 size - 1
     */
    unsigned long sizemask;

    /**
     * 哈希表中当前键值对的数量
     */
    unsigned long used;
} dictht;

/**
 * 字典
 */
typedef struct dict {
    /**
     * 字典类型, 根据不同的类型会调用不同的函数, 实现多态
     */
    dictType *type;

    /**
     * 私有数据
     */
    void *privdata;

    /**
     * 字典使用了两个哈希表, 因为 redis 使用的是渐进式哈希
     */
    dictht ht[2];

    /**
     * 如果没有处于 rehash, 那么这个变量的值是 -1, 否则它代表了当前
     * rehash 的位置
     */
    long rehashidx;

    /**
     * 当前迭代器的数量
     */
    int iterators;
} dict;

/**
 * 字典的迭代器
 */
typedef struct dictIterator {
    /**
     * 字典
     */
    dict *d;

    /**
     * 当前遍历的位置
     */
    long index;

    /**
     * 字典中有两个哈希表, 通过这个属性指明具体是哪个哈希表
     */
    int table;

    /**
     * 当前的遍历是否安全, 如果是 1 表示是安全的, 否则是不安全的.
     * 安全指的是可以在迭代过程中调用 dictAdd, dictFind 或者是
     * 其他的一些函数. 如果是不安全的, 那么在迭代过程中指可以调用
     * dictNext 函数.
     */
    int safe;

    /**
     * 当前的 Entry
     */
    dictEntry *entry;

    /**
     * 下一个 Entry
     */
    dictEntry *nextEntry;

    /**
     * 对于不安全迭代器所使用的指纹, 放置迭代过程中字典被修改
     */
    long long fingerprint;
} dictIterator;

/**
 * 迭代回调函数, 用于 dictScan
 */
typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

/**
 * 哈希表初始大小
 */
#define DICT_HT_INITIAL_SIZE     4

/**
 * 宏定义
 */

/**
 * 在字典中释放某个值
 */
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

/**
 * 在字典中设置某个 entry 的值
 */
#define dictSetVal(d, entry, _val_) \
do { \
    if ((d)->type->valDup) \
        entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->v.val = (_val_); \
} while(0)

/**
 * 将 entry 的值设置为有符号 int 型整数
 */
#define dictSetSignedIntegerVal(entry, _val_) \
    do { entry->v.s64 = _val_; } while(0)

/**
 * 将 entry 的值设置为无符号 int 型整数
 */
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { entry->v.u64 = _val_; } while(0)

/**
 * 将 entry 的值设置为双精度浮点数
 */
#define dictSetDoubleVal(entry, _val_) \
    do { entry->v.d = _val_; } while(0)

/**
 * 在字典中释放键
 */
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

/**
 * 在字典中设置某个 entry 的键
 */
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

/**
 * 比较两个键是否相同
 */
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

/**
 * 计算键所对应的哈希值
 */
#define dictHashKey(d, key) (d)->type->hashFunction(key)

/**
 * 获得 entry 中的键
 */
#define dictGetKey(he) ((he)->key)

/**
 * 获得 entry 中的值
 */
#define dictGetVal(he) ((he)->v.val)

/**
 * 获取 entry 中的值, 有符号 int
 */
#define dictGetSignedIntegerVal(he) ((he)->v.s64)

/**
 * 获取 entry 中的值, 无符号 int
 */
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)

/**
 * 获取 entry 中的值, 双精度浮点数
 */
#define dictGetDoubleVal(he) ((he)->v.d)

/**
 * 获取哈希表总大小
 */
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)

/**
 * 获取哈希表总键值对数量
 */
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)

/**
 * 判断字典是否处于 rehash
 */
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/**
 * API
 */

/**
 * 创建一个字典
 *
 * @param type 字典类型
 * @param privDataPtr 私有数据
 * @return 创建的字典
 */
dict *dictCreate(dictType *type, void *privDataPtr);

/**
 * 创建哈希表或者对哈希表进行扩容
 *
 * @param d 字典
 * @param size 大小
 * @return 处理结果
 */
int dictExpand(dict *d, unsigned long size);

/**
 * 向哈希表中新插入一个键值对
 *
 * @param d 字典
 * @param key 键
 * @param val 值
 * @return 处理结果
 */
int dictAdd(dict *d, void *key, void *val);

/**
 * 在哈希表中新建一个 entry, 并返回这个 entry 给调用方, 但是 entry 中
 * 并没有设置 value, 需要调用方自己来设置
 *
 * @param d 字典
 * @param key 键
 * @return 创建的 entry
 */
dictEntry *dictAddRaw(dict *d, void *key);

/**
 * 根据指定的键对哈希表中的值做更新
 *
 * @param d 字典
 * @param key 键
 * @param val 值
 * @return 处理结果
 */
int dictReplace(dict *d, void *key, void *val);

/**
 * 返回指定的键所对应的 entry, 如果找不到那么会新建一个
 *
 * @param d 字典
 * @param key 键
 * @return 找到的或者是新建的 entry
 */
dictEntry *dictReplaceRaw(dict *d, void *key);

/**
 * 在哈希表中删除指定的键值对
 *
 * @param d 字典
 * @param key 键
 * @return 处理结果
 */
int dictDelete(dict *d, const void *key);

/**
 * 在哈希表中删除指定的键值对, 并且不释放键和值的内存, 这个方法应该已经废弃了
 *
 * @param d
 * @param key
 * @return
 */
int dictDeleteNoFree(dict *d, const void *key);

/**
 * 释放哈希表的内存
 *
 * @param d 字典
 */
void dictRelease(dict *d);

/**
 * 在哈希表中根据指定的键查找键值对
 *
 * @param d 字典
 * @param key 键
 * @return entry
 */
dictEntry *dictFind(dict *d, const void *key);

/**
 * 在哈希表中根据指定的键查找键值对, 并返回这个键值对的值
 *
 * @param d 字典
 * @param key 键
 * @return 值
 */
void *dictFetchValue(dict *d, const void *key);

/**
 * 重新调整哈希表的大小, 这里的大小指的是能容纳所有键值对的最小大小
 *
 * @param d 字典
 * @return 处理结果
 */
int dictResize(dict *d);

/**
 * 获取迭代器
 *
 * @param d 字典
 * @return 迭代器
 */
dictIterator *dictGetIterator(dict *d);

/**
 * 获取一个安全的(safe = 1)迭代器
 *
 * @param d 字典
 * @return 安全的迭代器
 */
dictIterator *dictGetSafeIterator(dict *d);

/**
 * 获取要遍历的下一个 entry
 *
 * @param iter 迭代器
 * @return 下一个 entry
 */
dictEntry *dictNext(dictIterator *iter);

/**
 * 释放迭代器
 *
 * @param iter 迭代器
 */
void dictReleaseIterator(dictIterator *iter);

/**
 * 生成一个随机的 entry
 *
 * @param d 字典
 * @return 随机生成的 entry
 */
dictEntry *dictGetRandomKey(dict *d);

/**
 * 从哈希表中随机获取不多于 count 个 entry, 这些 entry 将通过参数 des 进行返回, 这个函数
 * 的返回值表示 des 实际的长度
 *
 * @param d 字典
 * @param des entry 数组
 * @param count 期望 entry 的个数
 * @return des 数组中 entry 的实际个数
 */
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);

/**
 * 打印字典信息
 *
 * @param d 字典
 */
void dictPrintStats(dict *d);

/**
 * 根据指定的键, 生成对应的哈希值, 这个函数是大小写敏感的, 在不同机器上生成的
 * 哈希值可能不一致
 *
 * @param key 键
 * @param len 键的长度
 * @return 哈希值
 */
unsigned int dictGenHashFunction(const void *key, int len);

/**
 * 根据指定的键, 生成对应的哈希值, 这个函数不是大小写敏感的
 *
 * @param buf 键
 * @param len 键的长度
 * @return 哈希值
 */
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);

/**
 * 清空哈希表
 *
 * @param d 字典
 * @param callback 回调函数
 */
void dictEmpty(dict *d, void(callback)(void *));

/**
 * 设置哈希表可以进行扩容
 */
void dictEnableResize(void);

/**
 * 禁止哈希表进行扩容
 */
void dictDisableResize(void);

/**
 * 渐进式 rehash
 *
 * @param d 字典
 * @param n 渐进式哈希的次数
 * @return 处理结果
 */
int dictRehash(dict *d, int n);

/**
 * 在指定的时间内进行渐进式rehash
 *
 * @param d 字典
 * @param ms 时间
 * @return rehash 的 entry 的个数
 */
int dictRehashMilliseconds(dict *d, int ms);

/**
 * 设置计算哈希时使用的种子
 *
 * @param initval 种子值
 */
void dictSetHashFunctionSeed(unsigned int initval);

/**
 * 获取当前设置的种子
 *
 * @return 当前设置的种子值
 */
unsigned int dictGetHashFunctionSeed(void);

/**
 * 遍历字典
 *
 * @param d 字典
 * @param v 游标
 * @param fn 每个 entry 要执行的操作
 * @param privdata 回调函数
 * @return 处理结果
 */
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata);

/**
 * DictType, 下面三个目前不会使用
 */

extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif
