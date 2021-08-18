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

dictEntry *dictNext(dictIterator *iter) {
    while (1) {
        if (iter->entry == NULL) {
            dictht *ht = &iter->d->ht[iter->table];
            if (iter->index == -1 && iter->table == 0) {
                if (iter->safe)
                    iter->d->iterators++;
                else
                    iter->fingerprint = dictFingerprint(iter->d);
            }
            iter->index++;
            if (iter->index >= (long) ht->size) {
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break;
                }
            }
            iter->entry = ht->table[iter->index];
        } else {
            iter->entry = iter->nextEntry;
        }
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

void dictReleaseIterator(dictIterator *iter) {
    if (!(iter->index == -1 && iter->table == 0)) {
        if (iter->safe)
            iter->d->iterators--;
        else {

        }
        // assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    zfree(iter);
}

/* Return a random entry from the hash table. Useful to
 * implement randomized algorithms */
dictEntry *dictGetRandomKey(dict *d) {
    dictEntry *he, *orighe;
    unsigned int h;
    int listlen, listele;

    if (dictSize(d) == 0) return NULL;
    if (dictIsRehashing(d)) _dictRehashStep(d);
    if (dictIsRehashing(d)) {
        do {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
            h = d->rehashidx + (random() % (d->ht[0].size +
                                            d->ht[1].size -
                                            d->rehashidx));
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                 d->ht[0].table[h];
        } while (he == NULL);
    } else {
        do {
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while (he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
    listlen = 0;
    orighe = he;
    while (he) {
        he = he->next;
        listlen++;
    }
    listele = random() % listlen;
    he = orighe;
    while (listele--) he = he->next;
    return he;
}

/* This function samples the dictionary to return a few keys from random
 * locations.
 *
 * It does not guarantee to return all the keys specified in 'count', nor
 * it does guarantee to return non-duplicated elements, however it will make
 * some effort to do both things.
 *
 * Returned pointers to hash table entries are stored into 'des' that
 * points to an array of dictEntry pointers. The array must have room for
 * at least 'count' elements, that is the argument we pass to the function
 * to tell how many random elements we need.
 *
 * The function returns the number of items stored into 'des', that may
 * be less than 'count' if the hash table has less than 'count' elements
 * inside, or if not enough elements were found in a reasonable amount of
 * steps.
 *
 * Note that this function is not suitable when you need a good distribution
 * of the returned items, but only when you need to "sample" a given number
 * of continuous elements to run some kind of algorithm or to produce
 * statistics. However the function is much faster than dictGetRandomKey()
 * at producing N elements. */
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count) {
    unsigned int j; /* internal hash table id, 0 or 1. */
    unsigned int tables; /* 1 or 2 tables? */
    unsigned int stored = 0, maxsizemask;
    unsigned int maxsteps;

    if (dictSize(d) < count) count = dictSize(d);
    maxsteps = count * 10;

    /* Try to do a rehashing work proportional to 'count'. */
    for (j = 0; j < count; j++) {
        if (dictIsRehashing(d))
            _dictRehashStep(d);
        else
            break;
    }

    tables = dictIsRehashing(d) ? 2 : 1;
    maxsizemask = d->ht[0].sizemask;
    if (tables > 1 && maxsizemask < d->ht[1].sizemask)
        maxsizemask = d->ht[1].sizemask;

    /* Pick a random point inside the larger table. */
    unsigned int i = random() & maxsizemask;
    unsigned int emptylen = 0; /* Continuous empty entries so far. */
    while (stored < count && maxsteps--) {
        for (j = 0; j < tables; j++) {
            /* Invariant of the dict.c rehashing: up to the indexes already
             * visited in ht[0] during the rehashing, there are no populated
             * buckets, so we can skip ht[0] for indexes between 0 and idx-1. */
            if (tables == 2 && j == 0 && i < (unsigned int) d->rehashidx) {
                /* Moreover, if we are currently out of range in the second
                 * table, there will be no elements in both tables up to
                 * the current rehashing index, so we jump if possible.
                 * (this happens when going from big to small table). */
                if (i >= d->ht[1].size) i = d->rehashidx;
                continue;
            }
            if (i >= d->ht[j].size) continue; /* Out of range for this table. */
            dictEntry *he = d->ht[j].table[i];

            /* Count contiguous empty buckets, and jump to other
             * locations if they reach 'count' (with a minimum of 5). */
            if (he == NULL) {
                emptylen++;
                if (emptylen >= 5 && emptylen > count) {
                    i = random() & maxsizemask;
                    emptylen = 0;
                }
            } else {
                emptylen = 0;
                while (he) {
                    /* Collect all the elements of the buckets found non
                     * empty while iterating. */
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count) return stored;
                }
            }
        }
        i = (i + 1) & maxsizemask;
    }
    return stored;
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
static unsigned long rev(unsigned long v) {
    unsigned long s = 8 * sizeof(v); // bit size; must be power of 2
    unsigned long mask = ~0;
    while ((s >>= 1) > 0) {
        mask ^= (mask << s);
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/* dictScan() is used to iterate over the elements of a dictionary.
 *
 * Iterating works the following way:
 *
 * 1) Initially you call the function using a cursor (v) value of 0.
 * 2) The function performs one step of the iteration, and returns the
 *    new cursor value you must use in the next call.
 * 3) When the returned cursor is 0, the iteration is complete.
 *
 * The function guarantees all elements present in the
 * dictionary get returned between the start and end of the iteration.
 * However it is possible some elements get returned multiple times.
 *
 * For every element returned, the callback argument 'fn' is
 * called with 'privdata' as first argument and the dictionary entry
 * 'de' as second argument.
 *
 * HOW IT WORKS.
 *
 * The iteration algorithm was designed by Pieter Noordhuis.
 * The main idea is to increment a cursor starting from the higher order
 * bits. That is, instead of incrementing the cursor normally, the bits
 * of the cursor are reversed, then the cursor is incremented, and finally
 * the bits are reversed again.
 *
 * This strategy is needed because the hash table may be resized between
 * iteration calls.
 *
 * dict.c hash tables are always power of two in size, and they
 * use chaining, so the position of an element in a given table is given
 * by computing the bitwise AND between Hash(key) and SIZE-1
 * (where SIZE-1 is always the mask that is equivalent to taking the rest
 *  of the division between the Hash of the key and SIZE).
 *
 * For example if the current hash table size is 16, the mask is
 * (in binary) 1111. The position of a key in the hash table will always be
 * the last four bits of the hash output, and so forth.
 *
 * WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
 *
 * If the hash table grows, elements can go anywhere in one multiple of
 * the old bucket: for example let's say we already iterated with
 * a 4 bit cursor 1100 (the mask is 1111 because hash table size = 16).
 *
 * If the hash table will be resized to 64 elements, then the new mask will
 * be 111111. The new buckets you obtain by substituting in ??1100
 * with either 0 or 1 can be targeted only by keys we already visited
 * when scanning the bucket 1100 in the smaller hash table.
 *
 * By iterating the higher bits first, because of the inverted counter, the
 * cursor does not need to restart if the table size gets bigger. It will
 * continue iterating using cursors without '1100' at the end, and also
 * without any other combination of the final 4 bits already explored.
 *
 * Similarly when the table size shrinks over time, for example going from
 * 16 to 8, if a combination of the lower three bits (the mask for size 8
 * is 111) were already completely explored, it would not be visited again
 * because we are sure we tried, for example, both 0111 and 1111 (all the
 * variations of the higher bit) so we don't need to test it again.
 *
 * WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
 *
 * Yes, this is true, but we always iterate the smaller table first, then
 * we test all the expansions of the current cursor into the larger
 * table. For example if the current cursor is 101 and we also have a
 * larger table of size 16, we also test (0)101 and (1)101 inside the larger
 * table. This reduces the problem back to having only one table, where
 * the larger one, if it exists, is just an expansion of the smaller one.
 *
 * LIMITATIONS
 *
 * This iterator is completely stateless, and this is a huge advantage,
 * including no additional memory used.
 *
 * The disadvantages resulting from this design are:
 *
 * 1) It is possible we return elements more than once. However this is usually
 *    easy to deal with in the application level.
 * 2) The iterator must return multiple elements per call, as it needs to always
 *    return all the keys chained in a given bucket, and all the expansions, so
 *    we are sure we don't miss keys moving during rehashing.
 * 3) The reverse cursor is somewhat hard to understand at first, but this
 *    comment is supposed to help.
 */
unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       void *privdata) {
    dictht *t0, *t1;
    const dictEntry *de;
    unsigned long m0, m1;

    if (dictSize(d) == 0) return 0;

    if (!dictIsRehashing(d)) {
        t0 = &(d->ht[0]);
        m0 = t0->sizemask;

        /* Emit entries at cursor */
        de = t0->table[v & m0];
        while (de) {
            fn(privdata, de);
            de = de->next;
        }

    } else {
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
        de = t0->table[v & m0];
        while (de) {
            fn(privdata, de);
            de = de->next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
        do {
            /* Emit entries at cursor */
            de = t1->table[v & m1];
            while (de) {
                fn(privdata, de);
                de = de->next;
            }

            /* Increment bits not covered by the smaller mask */
            v = (((v | m0) + 1) & ~m0) | (v & m0);

            /* Continue while bits covered by mask difference is non-zero */
        } while (v & (m0 ^ m1));
    }

    /* Set unmasked bits so incrementing the reversed cursor
     * operates on the masked bits of the smaller table */
    v |= ~m0;

    /* Increment the reverse cursor */
    v = rev(v);
    v++;
    v = rev(v);

    return v;
}

/* ------------------------- private functions ------------------------------ */

/* Expand the hash table if needed */
static int _dictExpandIfNeeded(dict *d) {
    /* Incremental rehashing already in progress. Return. */
    if (dictIsRehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used / d->ht[0].size > dict_force_resize_ratio)) {
        return dictExpand(d, d->ht[0].used * 2);
    }
    return DICT_OK;
}

/* Our hash table capability is a power of two */
static unsigned long _dictNextPower(unsigned long size) {
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX;
    while (1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned.
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table. */
static int _dictKeyIndex(dict *d, const void *key) {
    unsigned int h, idx, table;
    dictEntry *he;

    /* Expand the hash table if needed */
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;
    /* Compute the key hash value */
    h = dictHashKey(d, key);
    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        he = d->ht[table].table[idx];
        while (he) {
            if (dictCompareKeys(d, key, he->key))
                return -1;
            he = he->next;
        }
        if (!dictIsRehashing(d)) break;
    }
    return idx;
}

void dictEmpty(dict *d, void(callback)(void *)) {
    _dictClear(d, &d->ht[0], callback);
    _dictClear(d, &d->ht[1], callback);
    d->rehashidx = -1;
    d->iterators = 0;
}

void dictEnableResize(void) {
    dict_can_resize = 1;
}

void dictDisableResize(void) {
    dict_can_resize = 0;
}

#if 0

/* The following is code that we don't use for Redis currently, but that is part
of the library. */

/* ----------------------- Debugging ------------------------*/

#define DICT_STATS_VECTLEN 50
static void _dictPrintStatsHt(dictht *ht) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];

    if (ht->used == 0) {
        printf("No stats available for empty dictionaries\n");
        return;
    }

    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    for (i = 0; i < ht->size; i++) {
        dictEntry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while(he) {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
        if (chainlen > maxchainlen) maxchainlen = chainlen;
        totchainlen += chainlen;
    }
    printf("Hash table stats:\n");
    printf(" table size: %ld\n", ht->size);
    printf(" number of elements: %ld\n", ht->used);
    printf(" different slots: %ld\n", slots);
    printf(" max chain length: %ld\n", maxchainlen);
    printf(" avg chain length (counted): %.02f\n", (float)totchainlen/slots);
    printf(" avg chain length (computed): %.02f\n", (float)ht->used/slots);
    printf(" Chain length distribution:\n");
    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        printf("   %s%ld: %ld (%.02f%%)\n",(i == DICT_STATS_VECTLEN-1)?">= ":"", i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }
}

void dictPrintStats(dict *d) {
    _dictPrintStatsHt(&d->ht[0]);
    if (dictIsRehashing(d)) {
        printf("-- Rehashing into ht[1]:\n");
        _dictPrintStatsHt(&d->ht[1]);
    }
}

/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int _dictStringCopyHTHashFunction(const void *key)
{
    return dictGenHashFunction(key, strlen(key));
}

static void *_dictStringDup(void *privdata, const void *key)
{
    int len = strlen(key);
    char *copy = zmalloc(len+1);
    DICT_NOTUSED(privdata);

    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static int _dictStringCopyHTKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    DICT_NOTUSED(privdata);

    return strcmp(key1, key2) == 0;
}

static void _dictStringDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);

    zfree(key);
}

dictType dictTypeHeapStringCopyKey = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but does not auto-duplicate the key.
 * It's used for intepreter's shared strings. */
dictType dictTypeHeapStrings = {
    _dictStringCopyHTHashFunction, /* hash function */
    NULL,                          /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but also automatically handle dynamic
 * allocated C strings as values. */
dictType dictTypeHeapStringCopyKeyValue = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    _dictStringDup,                /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    _dictStringDestructor,         /* val destructor */
};
#endif
