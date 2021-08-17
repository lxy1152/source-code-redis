/* adlist.c - A generic doubly linked list implementation
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

#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/**
 * 创建一个链表
 *
 * @return 创建的链表
 */
list *listCreate(void) {
    struct list *list = zmalloc(sizeof(*list));
    if (list == NULL) {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/**
 * 释放链表
 *
 * @param list 要释放的链表
 */
void listRelease(list *list) {
    unsigned long len;
    listNode *current;
    listNode *next;

    current = list->head;
    len = list->len;
    while (len--) {
        next = current->next;
        // 先释放值, 再释放当前节点
        if (list->free) {
            list->free(current->value);
        }
        zfree(current);
        current = next;
    }
    // 最后释放链表
    zfree(list);
}

/**
 * 根据指定的 value 新增一个节点插入到链表的头部
 *
 * @param list 要插入到的链表
 * @param value 指定的值
 * @return 插入后的链表
 */
list *listAddNodeHead(list *list, void *value) {
    listNode *node = zmalloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->value = value;
    if (list->len == 0) {
        list->head = node;
        list->tail = node;
        node->prev = NULL;
        node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++;
    return list;
}

/**
 * 根据指定的 value 新增一个节点插入到链表的尾部
 *
 * @param list 要插入到的链表
 * @param value 指定的值
 * @return 插入后的链表
 */
list *listAddNodeTail(list *list, void *value) {
    listNode *node = zmalloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->value = value;
    if (list->len == 0) {
        list->head = node;
        list->tail = node;
        node->prev = NULL;
        node->next = NULL;
    } else {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++;
    return list;
}

/**
 * 在指定的节点前/后添加一个新的节点
 *
 * @param list 链表
 * @param old_node 指定的节点
 * @param value 新节点的值
 * @param after 插入方向, 前还是后
 * @return 插入后的链表
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node = zmalloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->value = value;
    if (after) {
        node->prev = old_node;
        node->next = old_node->next;
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        node->next = old_node;
        node->prev = old_node->prev;
        if (list->head == old_node) {
            list->head = node;
        }
    }
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    list->len++;
    return list;
}

/**
 * 在链表中删除指定的节点 node, 调用方需要负责释放 value
 *
 * @param list 链表
 * @param node 要删除的节点
 */
void listDelNode(list *list, listNode *node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }
    if (list->free) {
        list->free(node->value);
    }
    zfree(node);
    list->len--;
}

/**
 * 返回链表对应的迭代器, 通过 listNext() 函数获取下一个元素
 *
 * @param list 链表
 * @param direction 方向
 * @return 迭代器
 */
listIter *listGetIterator(list *list, int direction) {
    listIter *iter = zmalloc(sizeof(*iter));
    if (iter == NULL) {
        return NULL;
    }
    if (direction == AL_START_HEAD) {
        iter->next = list->head;
    } else {
        iter->next = list->tail;
    }
    iter->direction = direction;
    return iter;
}

/**
 * 释放指定的迭代器
 *
 * @param iter 迭代器
 */
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/**
 * 将迭代器重置为从头开始
 *
 * @param list 链表
 * @param li 迭代器
 */
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

/**
 * 将迭代器重置为从尾开始
 *
 * @param list 链表
 * @param li 迭代器
 */
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/**
 * 通过迭代器返回下一个元素, 可以通过 listDelNode() 删除对应的节点.
 * 举例:
 * @code
 * iter = listGetIterator(list, <direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 * @endcode
 *
 * @param iter 迭代器
 * @return 下一个元素
 */
listNode *listNext(listIter *iter) {
    listNode *current = iter->next;
    if (current != NULL) {
        if (iter->direction == AL_START_HEAD) {
            iter->next = current->next;
        } else {
            iter->next = current->prev;
        }
    }
    return current;
}

/**
 * 通过这个函数可以复制整个链表, 不论成功还是失败, 原始链表都不会进行修改.
 * 如果设置了 dup 函数, 那么会根据这个函数来复制 value, 否则是直接复制
 * 指针.
 *
 * @param orig 原始链表
 * @return 复制后的新链表
 */
list *listDup(list *orig) {
    // 新建一个链表
    list *copy = listCreate();
    if (copy == NULL) {
        return NULL;
    }

    // 复制原链表的三个函数
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;

    // 获取原始链表的迭代器并开始便利
    listIter *iter = listGetIterator(orig, AL_START_HEAD);
    listNode *node;
    while ((node = listNext(iter)) != NULL) {
        void *value;
        // 如果设置了 dup 函数, 那么使用 dup 函数进行复制
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                listReleaseIterator(iter);
                return NULL;
            }
        } else {
            value = node->value;
        }

        // 在尾部插入节点
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            listReleaseIterator(iter);
            return NULL;
        }
    }

    // 释放迭代器并返回链表
    listReleaseIterator(iter);
    return copy;
}

/**
 * 在链表中从头开始查找指定的键 key 所对应的节点, 如果找到了就返回这个
 * 节点, 否则返回 NULL. 如果实现了 match 方法, 那么会根据 match 方
 * 法来进行比较, 否则会直接比较指针.
 *
 * @param list 链表
 * @param key 要查找的键
 * @return 值所对应的节点或者是 NULL
 */
listNode *listSearchKey(list *list, void *key) {
    listIter *iter = listGetIterator(list, AL_START_HEAD);;
    listNode *node;
    while ((node = listNext(iter)) != NULL) {
        if (list->match) {
            if (list->match(node->value, key)) {
                listReleaseIterator(iter);
                return node;
            }
        } else {
            if (key == node->value) {
                listReleaseIterator(iter);
                return node;
            }
        }
    }
    listReleaseIterator(iter);
    return NULL;
}

/**
 * 返回链表中指定索引位置处的节点, 索引是从 0 开始的. 另外如果这个数
 * 是负数, 代表倒数.
 *
 * @param list 链表
 * @param index 索引值
 * @return 对应的节点
 */
listNode *listIndex(list *list, long index) {
    listNode *n;
    if (index < 0) {
        index = (-index) - 1;
        n = list->tail;
        while (index-- && n) {
            n = n->prev;
        }
    } else {
        n = list->head;
        while (index-- && n) {
            n = n->next;
        }
    }
    return n;
}

/**
 * 旋转链表, 将尾节点移动到头节点
 *
 * @param list 链表
 */
void listRotate(list *list) {
    listNode *tail = list->tail;
    if (listLength(list) <= 1) {
        return;
    }
    // 移动 tail 指针
    list->tail = tail->prev;
    list->tail->next = NULL;
    // 把 tail 加到 head 上
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}
