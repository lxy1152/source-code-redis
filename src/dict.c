/* Hash Tables Implementation.
 *
 * This file implements in memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto resize if needed
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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <ctype.h>

#include "dict.h"
#include "zmalloc.h"

/**
 * 通过这个值可以控制能否调整哈希表的大小, 这个值可以通过提供的 dictEnableResize() 以及
 * dictDisableResize() 函数进行调整. 这是为了使 copy-on-write(COW) 时有更好的性能,
 * 因为如果禁止调整大小, 那么就没有 rehash, 这样就认为没有修改, COW 就不需要进行复制.
 *
 * 但即便将这个值设置为了 0, 也不是说所有的调整就全都被禁止了. 如果每个桶中平均的数量大于
 * dict_force_resize_ratio, 那么也会强制进行扩容.
 */
static int dict_can_resize = 1;

/**
 * 强制扩容比例, 说明见上
 */
static unsigned int dict_force_resize_ratio = 5;

/**
 * 私有方法
 */

/**
 * 如果有需要就对哈希表进行扩容
 *
 * @param ht 字典
 * @return 处理结果
 */
static int _dictExpandIfNeeded(dict *ht);

/**
 * 哈希表的容量必须是 2^n, 通过这个函数计算离 size 最近的那个 2^n
 *
 * @param size 指定的大小
 * @return 真正使用的大小
 */
static unsigned long _dictNextPower(unsigned long size);

/**
 * 对应 key 的索引值
 *
 * @param ht 字典
 * @param key 键
 * @return 对应的索引值
 */
static int _dictKeyIndex(dict *ht, const void *key);

/**
 * 初始化字典
 *
 * @param ht 字典
 * @param type 字典类型
 * @param privDataPtr 私有数据
 * @return 0
 */
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/**
 * 一些哈希函数的实现
 */

/**
 * Thomas Wang 给出的一种对 32 位数字进行哈希的实现, 他认为一个好的哈希函数
 * 应该具备以下两个特征:
 *
 * 1. 可逆
 * 如果有 h(x) = y, 那么相应的可以通过 h'(y) 反向计算得出原始值 x, 实现 x
 * 和 y 的一一对应关系. 如果存在哈希冲突, 那么应该由调用方来进行处理.<br>
 * 为了实现可逆, 在函数中涉及的运算应该都是可逆运算. 比如最基本的加减乘除运算
 * 就是可逆运算, 因为已知结果和其中一个值就能反向得到另外一个值. 除此之外,
 * (x + c) + (x << n) = y 以及 (x + c) + (x >> n) = y 也是可逆运算,
 * 此外不做证明, 可以自己尝试一下.<br>
 * 另外异或, 取反运算也是可逆的.
 *
 * 2. 雪崩效应
 * 雪崩效应是指: 输入可能是产生了很微小的变化, 但是输出要产生非常大的变化.
 * Thomas Wang 认为输出至少要有一半以上的比特位发生变化. 一些常见的运算是很
 * 容易产生雪崩效应的, 比如:
 * - 加法: 01111 + 00001 = 10000
 * - 减法: 10000 - 00001 = 01111
 * - 乘除: 基于加减法实现, 所以也会有雪崩效应
 * - 移位: 00001111 << 2 = 00111100
 * - 取反: ~(1000) = 0111
 * - 异或: (1101) ^ (1010) = 0111
 *
 * 所以在具体的实现上, 也是遵守上面两个要求的: 这些运算既保证了可逆性又同时
 * 可以实现雪崩效应. 不过这个哈希函数虽然 Redis 内提供了但并没有使用.
 *
 * @param key 键
 * @return 哈希值
 */
unsigned int dictIntHashFunction(unsigned int key) {
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

/**
 * 哈希函数使用的种子
 */
static uint32_t dict_hash_function_seed = 5381;

/**
 * 设置种子值
 *
 * @param seed 种子值
 */
void dictSetHashFunctionSeed(uint32_t seed) {
    dict_hash_function_seed = seed;
}

/**
 * 获取种子值
 *
 * @return 种子值
 */
uint32_t dictGetHashFunctionSeed(void) {
    return dict_hash_function_seed;
}

/**
 * MurmurHash2 算法, 这个算法是对字符串进行哈希的. 这个函数会四个字节一组进行混合.<br>
 * 注意:
 * 1. 这个函数是大小写敏感的
 * 2. 在大端机器和小端机器上生成的哈希值可能不同
 *
 * @param key 键
 * @param len 长度
 * @return 哈希值
 */
unsigned int dictGenHashFunction(const void *key, int len) {
    // 因为这俩数表现的很好, 所以设置为这两个值
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    // 将结果初始化为一个随机值
    uint32_t h = dict_hash_function_seed ^ len;

    // 四个字节一组进行混合
    const unsigned char *data = (const unsigned char *) key;
    while (len >= 4) {
        uint32_t k = *(uint32_t *) data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // 如果不够 4 个字节, 处理剩下的那几个字节的内容
    switch (len) {
        case 3:
            h ^= data[2] << 16;
        case 2:
            h ^= data[1] << 8;
        case 1:
            h ^= data[0];
            h *= m;
    };

    // 最后再混合一下, 确保最后的几个字节也被混合了
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int) h;
}

/**
 * 基于 DJB 哈希算法实现的一个哈希函数. DJB 算法也称为 Times33 算法, 因为
 * 它的核心思想是乘 33 再加上当前的字符.<br>
 * 注意: 这个函数是大小写不敏感的.
 *
 * @param buf 键
 * @param len 长度
 * @return 哈希值
 */
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len) {
    unsigned int hash = (unsigned int) dict_hash_function_seed;

    while (len--) {
        // 这个计算的意思是: hash * 33 + c
        hash = ((hash << 5) + hash) + (tolower(*buf++));
    }
    return hash;
}

/**
 * API
 */

/**
 * 重置哈希表
 *
 * @param ht 哈希表
 */
static void _dictReset(dictht *ht) {
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/**
 * 新建一个字典
 *
 * @param type 字典类型
 * @param privDataPtr 可选参数
 * @return 创建的字典
 */
dict *dictCreate(dictType *type, void *privDataPtr) {
    dict *d = zmalloc(sizeof(*d));
    _dictInit(d, type, privDataPtr);
    return d;
}

/**
 * 初始化字典
 *
 * @param d 字典
 * @param type 字典类型
 * @param privDataPtr 可选数据
 * @return 处理结果
 */
int _dictInit(dict *d, dictType *type, void *privDataPtr) {
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);
    d->type = type;
    d->privdata = privDataPtr;
    d->rehashidx = -1;
    d->iterators = 0;
    return DICT_OK;
}

/**
 * 重新调整哈希表的大小, 这里的大小指的是能容纳所有键值对的最小大小
 *
 * @param d 字典
 * @return 处理结果
 */
int dictResize(dict *d) {
    // 如果不允许调整大小或者哈希表正处于 rehash, 那么返回失败
    if (!dict_can_resize || dictIsRehashing(d)) {
        return DICT_ERR;
    }
    // 设置最小大小并扩容
    int minimal = d->ht[0].used;
    if (minimal < DICT_HT_INITIAL_SIZE) {
        minimal = DICT_HT_INITIAL_SIZE;
    }
    return dictExpand(d, minimal);
}

/**
 * 创建哈希表或者对哈希表进行扩容
 *
 * @param d 字典
 * @param size 大小
 * @return 处理结果
 */
int dictExpand(dict *d, unsigned long size) {
    // 如果哈希表正在 rehash 或者元素数量比指定的多都拒绝
    if (dictIsRehashing(d) || d->ht[0].used > size) {
        return DICT_ERR;
    }

    // 根据给定的大小获取离它最近的 2^n
    unsigned long realsize = _dictNextPower(size);
    // 如果大小就没必要扩容
    if (realsize == d->ht[0].size) {
        return DICT_ERR;
    }

    // 新的哈希表
    dictht n;
    n.size = realsize;
    n.sizemask = realsize - 1;
    n.table = zcalloc(realsize * sizeof(dictEntry *));
    n.used = 0;

    // 如果是初始化, 那么不需要 rehash 就直接返回了
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    // 准备第二个哈希表用于渐进式哈希
    d->ht[1] = n;
    d->rehashidx = 0;
    return DICT_OK;
}

/**
 * 最多进行 n 次 rehash, 返回 1 表示 rehash 部分完成, 返回 0 表示 rehash 全部完成.
 *
 * @param d 字典
 * @param n 渐进式哈希的次数
 * @return 处理结果
 */
int dictRehash(dict *d, int n) {
    // 未处于 rehash 状态, 返回处理完成
    if (!dictIsRehashing(d)) {
        return 0;
    }
    // 最多访问空桶的数量
    int empty_visits = n * 10;

    while (n-- && d->ht[0].used != 0) {
        // 找到哈希表中不为空的那个桶
        while (d->ht[0].table[d->rehashidx] == NULL) {
            d->rehashidx++;
            if (--empty_visits == 0) {
                return 1;
            }
        }
        dictEntry *de = d->ht[0].table[d->rehashidx];

        // 将这个桶中的键值对移动到新的哈希表中
        while (de) {
            dictEntry *nextde = de->next;
            // 计算索引, 计算方式是 (size - 1) & hash
            // 只有 size 是 2^n 的时候, 才可以使用与运算来替代求余运算
            unsigned int h = dictHashKey(d, de->key) & d->ht[1].sizemask;
            // 头插法
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            d->ht[0].used--;
            d->ht[1].used++;
            de = nextde;
        }

        // 原哈希表的桶置为 NULL
        d->ht[0].table[d->rehashidx] = NULL;
        // rehash 索引 + 1
        d->rehashidx++;
    }

    // 检查是不是整个哈希表都 rehash 完成了
    if (d->ht[0].used == 0) {
        zfree(d->ht[0].table);
        d->ht[0] = d->ht[1];
        _dictReset(&d->ht[1]);
        d->rehashidx = -1;
        return 0;
    }

    return 1;
}

/**
 * 获取当前时间, 单位是毫秒
 *
 * @return 当前时间
 */
long long timeInMilliseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((long long) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

/**
 * 在指定的时间段内进行 rehash 操作
 *
 * @param d 字典
 * @param ms 时间
 * @return 处理结果
 */
int dictRehashMilliseconds(dict *d, int ms) {
    // 开始时间
    long long start = timeInMilliseconds();
    // 执行次数
    int rehashes = 0;
    // 每次执行 100 次, 超过时间限制就退出
    while (dictRehash(d, 100)) {
        rehashes += 100;
        if (timeInMilliseconds() - start > ms) {
            break;
        }
    }
    return rehashes;
}

/**
 * 进行一次 rehash, 这个方法会在 dictAddRaw, dictFind, dictGenericDelete,
 * dictGetRandomKey, dictGetSomeKeys 中调用
 *
 * @param d 字典
 */
static void _dictRehashStep(dict *d) {
    // 非迭代的情况下, 才能进行 rehash
    if (d->iterators == 0) {
        dictRehash(d, 1);
    }
}

/**
 * 在字典中插入一个键值对
 *
 * @param d 字典
 * @param key 键
 * @param val 值
 * @return 处理结果
 */
/* Add an element to the target hash table */
int dictAdd(dict *d, void *key, void *val) {
    // 通过 dictAddRaw 获得创建的 dictEntry
    dictEntry *entry = dictAddRaw(d, key);
    if (!entry) {
        return DICT_ERR;
    }
    // 再设置值
    dictSetVal(d, entry, val);
    return DICT_OK;
}

/**
 * 根据指定的键创建一个 dictEntry, 把设置值的步骤交给调用方来处理.
 *
 * @param d 字典
 * @param key 键
 * @return 创建的 entry
 */
dictEntry *dictAddRaw(dict *d, void *key) {
    // 如果正处于 rehash, 那么执行一次 rehash
    if (dictIsRehashing(d)) {
        _dictRehashStep(d);
    }

    // 获取键所对应的节点的索引, -1 代表已存在
    int index = _dictKeyIndex(d, key);
    if (index == -1) {
        return NULL;
    }

    // 根据是否处于 rehash, 选择不同的哈希表
    dictht *ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
    dictEntry *entry = zmalloc(sizeof(*entry));
    // 头插法
    entry->next = ht->table[index];
    ht->table[index] = entry;
    ht->used++;

    // 设置键
    dictSetKey(d, entry, key);

    // 返回 entry
    return entry;
}

/**
 * 尝试插入或者替换键值对
 *
 * @param d 字典
 * @param key 键
 * @param val 值
 * @return 处理结果
 */
int dictReplace(dict *d, void *key, void *val) {
    // 先试试能不能成功插入
    if (dictAdd(d, key, val) == DICT_OK) {
        return 1;
    }

    // 根据键获取对应的 entry
    dictEntry *entry = dictFind(d, key);
    dictEntry auxentry = *entry;

    // 设置值并释放旧 entry 的内存
    dictSetVal(d, entry, val);
    dictFreeVal(d, &auxentry);
    return 0;
}

/**
 * 获取要进行替换的 entry, 如果找不到那么会新建一个 entry. 至于要如何替换值,
 * 则交给调用方来考虑.
 *
 * @param d 字典
 * @param key 键
 * @return entry
 */
dictEntry *dictReplaceRaw(dict *d, void *key) {
    dictEntry *entry = dictFind(d, key);
    return entry ? entry : dictAddRaw(d, key);
}

/**
 * 在字典中删除键所对应的键值对
 *
 * @param d 字典
 * @param key 键
 * @param nofree 是否释放键和值的内存
 * @return 处理结果
 */
static int dictGenericDelete(dict *d, const void *key, int nofree) {
    // 哈希表为空
    if (d->ht[0].size == 0) {
        return DICT_ERR;
    }
    // 如果正处于 rehash 状态, 那么进行一次 rehash
    if (dictIsRehashing(d)) {
        _dictRehashStep(d);
    }

    // 计算键对应的哈希值
    unsigned int h = dictHashKey(d, key);
    // 桶索引
    unsigned int idx;
    // 当前遍历的那个节点
    dictEntry *he;
    // 遍历的这个节点的前一个节点
    dictEntry *prevHe;

    // 遍历两个哈希表
    for (int table = 0; table <= 1; table++) {
        // (size - 1) & hash
        idx = h & d->ht[table].sizemask;
        // 取链表头节点
        he = d->ht[table].table[idx];
        // 前一节点设置为 NULL
        prevHe = NULL;

        // 遍历链表
        while (he) {
            // 比较键是否相同
            if (dictCompareKeys(d, key, he->key)) {
                // 从链表中删除节点
                if (prevHe) {
                    prevHe->next = he->next;
                } else {
                    d->ht[table].table[idx] = he->next;
                }

                // 释放键和值的内存
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                }

                // 释放节点
                zfree(he);

                // 数量减一并返回
                d->ht[table].used--;
                return DICT_OK;
            }

            // 设置前一个节点
            prevHe = he;
            he = he->next;
        }

        // 如果不是处于 rehash, 那么就不需要遍历第二个哈希表
        if (!dictIsRehashing(d)) {
            break;
        }
    }

    // 找不到 entry
    return DICT_ERR;
}

/**
 * 删除指定键所对应的 entry, 这个方法会释放键和值的内存
 *
 * @param ht 字典
 * @param key 键
 * @return 处理结果
 */
int dictDelete(dict *ht, const void *key) {
    // 调用 dictGenericDelete 进行删除, 需要释放内存
    return dictGenericDelete(ht, key, 0);
}

/**
 * 删除指定键所对应的 entry, 这个方法不会释放键和值的内存.
 * 注意: 这个方法应该是废弃的
 *
 * @param ht 字典
 * @param key 键
 * @return 处理结果
 */
int dictDeleteNoFree(dict *ht, const void *key) {
    return dictGenericDelete(ht, key, 1);
}

/**
 * 清空哈希表
 *
 * @param d 字典
 * @param ht 哈希表
 * @param callback 回调函数
 * @return 处理结果
 */
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {
    // 释放所有元素
    for (unsigned long i = 0; i < ht->size && ht->used > 0; i++) {
        // 每 65535 个间隔执行一次回调函数
        if (callback && (i & 65535) == 0) {
            callback(d->privdata);
        }
        dictEntry *he = ht->table[i];
        // 桶为空
        if ((he = ht->table[i]) == NULL) {
            continue;
        }
        // 遍历释放内存
        dictEntry *nextHe;
        while (he) {
            nextHe = he->next;
            dictFreeKey(d, he);
            dictFreeVal(d, he);
            zfree(he);
            ht->used--;
            he = nextHe;
        }
    }

    // 释放哈希表
    zfree(ht->table);
    _dictReset(ht);
    return DICT_OK;
}

/**
 * 释放字典
 *
 * @param d 字典
 */
void dictRelease(dict *d) {
    _dictClear(d, &d->ht[0], NULL);
    _dictClear(d, &d->ht[1], NULL);
    zfree(d);
}

/**
 * 在字典中根据指定的键查找对应的 entry
 *
 * @param d 字典
 * @param key 键
 * @return entry
 */
dictEntry *dictFind(dict *d, const void *key) {
    // 哈希表为空
    if (d->ht[0].size == 0) {
        return NULL;
    }
    // 如果是处于 rehash 状态, 则执行一次 rehash
    if (dictIsRehashing(d)) {
        _dictRehashStep(d);
    }
    // 计算哈希值
    unsigned int h = dictHashKey(d, key);
    // 遍历两个哈希表
    for (int table = 0; table <= 1; table++) {
        // (size - 1) & hash
        unsigned int idx = h & d->ht[table].sizemask;
        // 遍历链表查找
        dictEntry *he = d->ht[table].table[idx];
        while (he) {
            if (dictCompareKeys(d, key, he->key)) {
                return he;
            }
            he = he->next;
        }
        // 如果没有处于 rehash, 那么就没有必要再遍历另一个哈希表了
        if (!dictIsRehashing(d)) {
            return NULL;
        }
    }
    return NULL;
}

/**
 * 从字典获取键所对应的值
 *
 * @param d 字典
 * @param key 键
 * @return 值
 */
void *dictFetchValue(dict *d, const void *key) {
    dictEntry *he = dictFind(d, key);
    return he ? dictGetVal(he) : NULL;
}

/**
 * 为字典生成一个指纹. 指纹是一个 64 位的数, 它可以表示字典在某个时刻的状态. 当使用一个不安全的
 * 迭代器时, 程序会生成一个指针, 同时在释放迭代器的时候再次检查指纹. 如果两次指纹不相同, 就说明
 * 在迭代的过程中调用了一些不支持的方法.
 *
 * @param d 字典
 * @return 指纹
 */
long long dictFingerprint(dict *d) {
    long long integers[6];
    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    // 下面的结果 = hash(hash(hash(int1) + int2) + int3) ...
    // 通过这种操作在很大程度上会生成不同的指纹
    // 计算哈希的方式是: Thomas Wang 的 64 位版本
    // 可以参考 dictIntHashFunction 函数
    long long hash = 0;
    for (int j = 0; j < 6; j++) {
        hash += integers[j];

        // 计算哈希
        hash = (~hash) + (hash << 21);
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8);
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4);
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

/**
 * 获取一个不安全的迭代器
 *
 * @param d 字典
 * @return 不安全的迭代器
 */
dictIterator *dictGetIterator(dict *d) {
    dictIterator *iter = zmalloc(sizeof(*iter));

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

/**
 * 获取一个安全的迭代器
 *
 * @param d 字典
 * @return 安全的迭代器
 */
dictIterator *dictGetSafeIterator(dict *d) {
    dictIterator *i = dictGetIterator(d);

    i->safe = 1;
    return i;
}

/**
 * 获取下一个 entry
 *
 * @param iter 迭代器
 * @return 下一个 entry
 */
dictEntry *dictNext(dictIterator *iter) {
    while (1) {
        if (iter->entry == NULL) {
            // 获取哈希表
            dictht *ht = &iter->d->ht[iter->table];

            // 如果是刚开始遍历第一个哈希表
            if (iter->index == -1 && iter->table == 0) {
                // 对于安全的迭代器, 把数量加一
                // 对于不安全的迭代器, 生成字典指纹
                if (iter->safe) {
                    iter->d->iterators++;
                } else {
                    iter->fingerprint = dictFingerprint(iter->d);
                }
            }

            // 索引加一
            iter->index++;
            // 如果这个哈希表已经遍历完了
            if (iter->index >= (long) ht->size) {
                // 如果哈希表正在 rehash, 继续遍历第二个哈希表
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break;
                }
            }
            // 设置当前遍历到的 entry
            iter->entry = ht->table[iter->index];
        } else {
            iter->entry = iter->nextEntry;
        }

        if (iter->entry) {
            // 需要保存 next, 因为这个节点可能被调用方删除
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

/**
 * 释放迭代器
 *
 * @param iter 迭代器
 */
void dictReleaseIterator(dictIterator *iter) {
    if (!(iter->index == -1 && iter->table == 0)) {
        // 对于安全的迭代器需要减少迭代器数量
        // 对于不安全的迭代器需要验证指纹是否匹配
        if (iter->safe) {
            iter->d->iterators--;
        } else if (iter->fingerprint != dictFingerprint(iter->d)) {
            printf("\033[0;31mfingerprints do not match\033[0m\n");
            // 将一个负地址位置的值设置为 x, 这显然是无法执行的
            // 这会触发 SIGSEGV 信号
            *((char *) -1) = 'x';
        }
    }
    zfree(iter);
}

/**
 * 从哈希表中返回一个随机的 entry
 *
 * @param d 字典
 * @return entry
 */
dictEntry *dictGetRandomKey(dict *d) {
    // 哈希表是空的
    if (dictSize(d) == 0) {
        return NULL;
    }
    // 如果正处于 rehash, 先进行一次 rehash
    if (dictIsRehashing(d)) {
        _dictRehashStep(d);
    }

    // 随机获取的那个 entry
    dictEntry *he;
    // 索引
    unsigned int h;

    if (dictIsRehashing(d)) {
        do {
            // rehash 的时候 0 ~ rehashidx - 1 是没有元素的
            h = d->rehashidx + (random() % (d->ht[0].size + d->ht[1].size - d->rehashidx));
            // 根据当前 rehash 的程度, 选择从哪个哈希表中取数据
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] : d->ht[0].table[h];
        } while (he == NULL);
    } else {
        // 非 rehash 时直接从第一个哈希表中随机获取
        do {
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while (he == NULL);
    }

    // 现在只是找到了一个链表, 需要从这个链表中随机获取一个节点
    // 链表长度
    int listlen = 0;
    // 链表的头节点
    dictEntry *orighe = he;
    // 计算链表长度
    while (he) {
        he = he->next;
        listlen++;
    }
    // 计算随机索引
    int listele = random() % listlen;
    // 根据索引获取那个节点
    he = orighe;
    while (listele--) {
        he = he->next;
    }
    return he;
}

/**
 * 这个函数会返回字典中的一些随机键, 但不保证:
 * 1. 返回的键的数量一定等于指定的数量 count
 * 2. 都是无重复的键
 * 只是会尽力去避免.
 *
 * 返回的 entry 保存在参数 des 中, 所以调用方需要保证 des 有足够的空间来保存.
 * des 中返回的个数并不一定就等于 count, 有可能哈希表压根就没有这么多元素, 或
 * 者是按照查询的步骤查不出这么多的元素.
 *
 * 注意:
 * 1. 这个函数适合于某些需要连续元素的样本的算法, 但不保证它们有良好的分布
 * 2. 这个函数比 dictGetRandomKey 要快
 *
 * @param d 字典
 * @param des 随机获取的一些 entry
 * @param count 期望的个数
 * @return 实际返回的个数
 */
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count) {
    // count 不可以比哈希表当前的大小还要大
    if (dictSize(d) < count) {
        count = dictSize(d);
    }
    // 进行 rehash
    for (int j = 0; j < count; j++) {
        if (dictIsRehashing(d)) {
            _dictRehashStep(d);
        } else {
            break;
        }
    }
    // 要遍历一个还是两个哈希表
    unsigned int tables = dictIsRehashing(d) ? 2 : 1;
    // 取掩码
    unsigned int maxsizemask = d->ht[0].sizemask;
    if (tables > 1 && maxsizemask < d->ht[1].sizemask) {
        maxsizemask = d->ht[1].sizemask;
    }

    // 随便取一个随机索引
    unsigned int i = random() & maxsizemask;
    // 连续空桶的数量
    unsigned int emptylen = 0;
    // 实际随机获取的 entry 的数量
    unsigned int stored = 0;
    // 最大执行次数
    unsigned int maxsteps = count * 10;

    while (stored < count && maxsteps--) {
        for (int j = 0; j < tables; j++) {
            // 由于存在 rehash, 如果第一个哈希表的 0 ~ rehashidx - 1 之间的索引
            // 位置都是空的, 直接跳过
            if (tables == 2 && j == 0 && i < (unsigned int) d->rehashidx) {
                // 如果越界了, 就重新设置到 rehashidx 处
                if (i >= d->ht[1].size) {
                    i = d->rehashidx;
                }
                continue;
            }
            // 越界
            if (i >= d->ht[j].size) {
                continue;
            }

            // 取链表
            dictEntry *he = d->ht[j].table[i];
            // 如果连续碰见 5 个空桶, 就重新找个别的地方
            if (he == NULL) {
                emptylen++;
                if (emptylen >= 5 && emptylen > count) {
                    i = random() & maxsizemask;
                    emptylen = 0;
                }
            } else {
                // 重置 emptylen
                emptylen = 0;
                // 把链表保存到 des 中
                while (he) {
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count) {
                        return stored;
                    }
                }
            }
        }

        // 重新计算 i
        i = (i + 1) & maxsizemask;
    }

    // des 中实际的个数
    return stored;
}

/**
 * 对 v 进行二进制逆序, 参考: http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel
 *
 * @param v 要翻转的数字
 * @return 翻转后的数字
 */
static unsigned long rev(unsigned long v) {
    unsigned long s = 8 * sizeof(v);
    unsigned long mask = ~0;
    while ((s >>= 1) > 0) {
        mask ^= (mask << s);
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/**
 * 这个函数是用来遍历整个字典的, 迭代的过程如下:
 * - 初始调用时使用一个游标, 游标的值是 0
 * - 这个函数会进行一次迭代并返回下次遍历时需要使用的游标的值
 * - 当返回的游标的值为 0 时, 表示迭代已经完成
 *
 * 这个函数保证会返回字典中的所有元素, 但有些元素可能会访问多次. 对于其中的每个键值对,
 * 都会对它调用传入的函数 fn, fn 的第一个参数需要接收 privdata, 第二个参数需要接收
 * 一个 entry(变量为 de).
 *
 * 这个迭代算法是由 Pieter Noordhuis 设计的, 主要思想是从游标的高位开始递增游标.
 * 不同于常规的递增方式, 需要先翻转游标的二进制位, 再进行递增, 最后再重新翻转回来. 而
 * 之所以需要这种操作是因为在迭代的过程中哈希表可能会进行 rehash.
 *
 * 这个文件中提供的哈希表实现, 与 Java 的 HashMap 类似. 它的大小永远是 2 的 n 次幂,
 * 可以使用与运算来替代求余运算. 举个例子, 如果哈希表的大小是 16, 掩码是 15, 二进制
 * 表示是 1111, 那么计算结果将仅依赖于掩码的低位.
 *
 * 在进行扩容操作时, 掩码将相应的增大. 比如大小为 32 时, 掩码为 31, 二进制表示是
 * 11111. 大小为 64 时, 掩码为 63, 二进制表示是 111111. 元素将通过 hash & sizemask
 * 重新计算索引, 此处哈希值是一样的, 那么随着掩码的增大, 索引值也将增大.
 *
 * 举个例子, 如果哈希值为 10, 10 & 15 = 10 = 1010, 10 & 7 = 2 = 10, 它们的低
 * 位是相同的, 仅仅在高位上新增了几位(新增的可能是全为 0). 即新的索引值为 ??10, 其中
 * ? 表示 0 或者 1.
 *
 * 再来看一下大小为 8 或 16 时, 游标的变化:
 * - 000 --> 100 --> 010 --> 110 --> 001 --> 101 --> 011 --> 111 --> 000
 * - 0000 --> 1000 --> 0100 --> 1100 --> 0010 --> 1010 --> 0110 --> 1110 --> 0001 --> 1001 --> 0101 --> 1101 -->
 *   0011 --> 1011 --> 0111 --> 1111 --> 0000
 *
 * 在扩容时, 索引值将被重新映射到 2i 和 2i + 1 的位置上. 010 将被映射到 0010 的位
 * 置上, 在它前面的四个值已经被遍历过了, 在它后面的是还没遍历的. 缩小的情况是相同的.
 *
 * 大概是这么个意思, 有个再细看看, 牛逼就完事了.
 * 可以参考: https://blog.csdn.net/gqtcgq/article/details/50533336
 *
 * @param d 字典
 * @param v 当前的游标
 * @param fn 要当前元素要指定的操作
 * @param privdata 可选数据
 * @return 下一次要访问的游标
 */
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata) {
    // 字典为空则直接返回
    if (dictSize(d) == 0) {
        return 0;
    }

    // 两个哈希表
    dictht *t0;
    dictht *t1;
    // 两个哈希表的掩码
    unsigned long m0;
    unsigned long m1;
    // 当前的 entry
    const dictEntry *de;

    // 判断是否处于 rehash 状态
    if (!dictIsRehashing(d)) {
        t0 = &(d->ht[0]);
        m0 = t0->sizemask;

        // 遍历游标位置处的所有 entry
        de = t0->table[v & m0];
        while (de) {
            fn(privdata, de);
            de = de->next;
        }
    } else {
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        // 确保 t0 是比较小的那个哈希表
        if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

        m0 = t0->sizemask;
        m1 = t1->sizemask;

        // 遍历游标位置处的所有 entry
        de = t0->table[v & m0];
        while (de) {
            fn(privdata, de);
            de = de->next;
        }

        // 遍历大表, 比如对于 ??010, 任取 0 或 1 开始遍历
        do {
            de = t1->table[v & m1];
            while (de) {
                fn(privdata, de);
                de = de->next;
            }

            v = (((v | m0) + 1) & ~m0) | (v & m0);
        } while (v & (m0 ^ m1));
    }

    // 保留游标的低 n 位数
    v |= ~m0;

    // 翻转
    v = rev(v);
    // 翻转后的低位加一
    v++;
    // 再转回来, 相当于高位加一并向低位进位
    v = rev(v);

    return v;
}


/**
 * 如果需要的话就对哈希表进行扩容
 *
 * @param d 字典
 * @return 处理结果
 */
static int _dictExpandIfNeeded(dict *d) {
    // 处于渐进式哈希时直接返回
    if (dictIsRehashing(d)) {
        return DICT_OK;
    }

    // 哈希表为空则进行初始化
    if (d->ht[0].size == 0) {
        return dictExpand(d, DICT_HT_INITIAL_SIZE);
    }

    // 如果使用允许扩容, 且已使用 / 总大小的比例至少达到了 1:1
    // 或者达到了强制扩容比例(默认为 5 : 1) 则进行扩容
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize || d->ht[0].used / d->ht[0].size > dict_force_resize_ratio)) {
        return dictExpand(d, d->ht[0].used * 2);
    }

    return DICT_OK;
}

/**
 * 返回离指定大小最近的 2 的 n 次方
 *
 * @param size 指定的大小
 * @return 离它最近的 2 的 n 次方
 */
static unsigned long _dictNextPower(unsigned long size) {
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) {
        return LONG_MAX;
    }
    while (1) {
        if (i >= size) {
            return i;
        }
        i *= 2;
    }
}

/**
 * 返回对应的键的索引值
 *
 * @param d 字典
 * @param key 键
 * @return 索引值
 */
static int _dictKeyIndex(dict *d, const void *key) {
    if (_dictExpandIfNeeded(d) == DICT_ERR) {
        return -1;
    }

    // 键的哈希值
    unsigned int h = dictHashKey(d, key);
    // 索引值
    unsigned int idx;
    // entry
    dictEntry *he;
    for (unsigned int table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        he = d->ht[table].table[idx];
        while (he) {
            if (dictCompareKeys(d, key, he->key)) {
                return -1;
            }
            he = he->next;
        }
        if (!dictIsRehashing(d)) {
            break;
        }
    }
    return idx;
}

/**
 * 清空哈希表
 *
 * @param d 字典
 * @param callback 回调函数
 */
void dictEmpty(dict *d, void(callback)(void *)) {
    _dictClear(d, &d->ht[0], callback);
    _dictClear(d, &d->ht[1], callback);
    d->rehashidx = -1;
    d->iterators = 0;
}

/**
 * 允许字典进行扩容
 */
void dictEnableResize(void) {
    dict_can_resize = 1;
}

/**
 * 进制字典进行扩容
 */
void dictDisableResize(void) {
    dict_can_resize = 0;
}
