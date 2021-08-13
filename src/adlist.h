/* adlist.h - A generic doubly linked list implementation
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/**
 * 链表节点
 */
typedef struct listNode {
    /**
     * 前驱节点
     */
    struct listNode *prev;

    /**
     * 后继节点
     */
    struct listNode *next;

    /**
     * 这个节点保存的值
     */
    void *value;
} listNode;


/**
 * 迭代器
 */
typedef struct listIter {
    /**
     * 下一节点
     */
    listNode *next;

    /**
     * 方向
     */
    int direction;
} listIter;

/**
 * 双向链表
 */
typedef struct list {
    /**
     * 头节点
     */
    listNode *head;

    /**
     * 尾节点
     */
    listNode *tail;

    /**
     * 复制值
     *
     * @param ptr 函数指针
     * @return 复制的值
     */
    void *(*dup)(void *ptr);

    /**
     * 释放节点值
     *
     * @param ptr 函数指针
     */
    void (*free)(void *ptr);

    /**
     * 比较值
     *
     * @param ptr 函数指针
     * @param key 值
     * @return 比较的结果
     */
    int (*match)(void *ptr, void *key);

    /**
     * 链表长度
     */
    unsigned long len;
} list;

/**
 * 通过宏定义的函数, 不包含:
 * - 获取节点的前驱节点
 * - 获取链表的复制, 释放, 比较值的函数指针
 */

/**
 * 获取链表长度
 */
#define listLength(l) ((l)->len)

/**
 * 获取链表头节点
 */
#define listFirst(l) ((l)->head)

/**
 * 获取链表尾节点
 */
#define listLast(l) ((l)->tail)

/**
 * 获取节点的后继节点
 */
#define listNextNode(n) ((n)->next)

/**
 * 获取节点的值
 */
#define listNodeValue(n) ((n)->value)

/**
 * 设置复制函数
 */
#define listSetDupMethod(l, m) ((l)->dup = (m))

/**
 * 设置释放函数
 */
#define listSetFreeMethod(l, m) ((l)->free = (m))

/**
 * 释放比较函数
 */
#define listSetMatchMethod(l, m) ((l)->match = (m))

/**
 * 创建一个链表
 *
 * @return 创建的链表
 */
list *listCreate(void);

/**
 * 释放指定链表
 *
 * @param list 要释放的链表
 */
void listRelease(list *list);

/**
 * 给链表添加一个头节点
 *
 * @param list 链表
 * @param value 节点的值
 * @return 插入后的链表
 */
list *listAddNodeHead(list *list, void *value);

/**
 * 给链表添加一个尾节点
 *
 * @param list 链表
 * @param value 节点的值
 * @return 插入后的链表
 */
list *listAddNodeTail(list *list, void *value);

/**
 * 在指定的节点前/后添加一个新的节点
 *
 * @param list 链表
 * @param old_node 指定的节点
 * @param value 新节点的值
 * @param after 插入方向, 前还是后
 * @return 插入后的链表
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after);

/**
 * 在链表中释放指定的节点
 *
 * @param list 链表
 * @param node 要删除的节点
 */
void listDelNode(list *list, listNode *node);

/**
 * 获取链表的迭代器
 *
 * @param list 链表
 * @param direction 迭代方向
 * @return 迭代器
 */
listIter *listGetIterator(list *list, int direction);

/**
 * 返回迭代器要访问的下一个链表节点
 *
 * @param iter 迭代器
 * @return 下一个链表节点
 */
listNode *listNext(listIter *iter);

/**
 * 释放指定的迭代器
 *
 * @param iter 要释放的迭代器
 */
void listReleaseIterator(listIter *iter);

/**
 * 复制传入的链表
 *
 * @param orig 原始链表
 * @return 复制后的链表
 */
list *listDup(list *orig);

/**
 * 在链表中查找指定值所对应的节点
 *
 * @param list 链表
 * @param key 要查找的值
 * @return 这个值对应的链表节点
 */
listNode *listSearchKey(list *list, void *key);

/**
 * 在链表中根据指定的索引值查找对应的节点
 *
 * @param list 链表
 * @param index 指定的索引值
 * @return 对应的节点
 */
listNode *listIndex(list *list, long index);

/**
 * 重新设置迭代器, 从头节点开始
 *
 * @param list 链表
 * @param li 迭代器
 */
void listRewind(list *list, listIter *li);

/**
 * 重新设置迭代器, 从尾节点开始
 *
 * @param list 链表
 * @param li 迭代器
 */
void listRewindTail(list *list, listIter *li);

/**
 * 旋转链表, 将尾节点插入到头节点之前
 *
 * @param list 链表
 */
void listRotate(list *list);

/**
 * 迭代器的遍历方向
 */

/**
 * 从头节点开始
 */
#define AL_START_HEAD 0

/**
 * 从尾节点开始
 */
#define AL_START_TAIL 1

#endif
