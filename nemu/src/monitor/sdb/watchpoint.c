/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
    int NO;
    struct watchpoint *next;

    /* TODO: Add more members if necessary */
    char *expr;
    word_t old_val;

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
    int i;
    for (i = 0; i < NR_WP; i++) {
        wp_pool[i].NO = i;
        wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    }

    head = NULL;
    free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

// 从free_链表中返回一个空闲的监视点结构
WP *new_wp() {
    assert(free_ != NULL);
    WP *p = free_;
    free_ = free_->next;
    return p;
}

// 将wp归还到free_链表
void free_WP(WP *p) {
    // 检测p的合法性
    assert(p >= wp_pool && p < wp_pool + NR_WP);
    free(p->expr);
    p->next = free_;
    free_ = p;
}

// 设置监测点
int set_watchpoint(char *e) {
    bool success;
    word_t val = expr(e, &success);
    if (!success) {
        return -1;
    }

    WP *p = new_wp();
    p->expr = e;
    p->old_val = val;

    // 节点插入head链表
    p->next = head;
    head = p;
    return p->NO;
}

// 删除监测点
bool delete_watchpoint(int NO) {
    WP *p, *prev = NULL;
    for (p = head; p != NULL; prev = p, p = p->next) {
        if (p->NO == NO) {
            break;
        }
    }

    if (p == NULL) {
        return false;
    }
    if (prev == NULL) {
        head = p->next;
    }
    else {
        prev->next = p->next;
    }

    free_WP(p);
    return true;
}

// 打印监测点
void list_watchpoint() {
    if (head == NULL) {
        printf("No watchpoints\n");
        return;
    }
    printf("%8s\t%8s\t%8s\n", "NO", "Address", "Enable");
    WP *p;
    for (p = head; p != NULL; p = p->next) {
        printf("%8d\t%s\t" FMT_WORD "\n", p->NO, p->expr, p->old_val);
    }
}

// 扫描监测点是否变更
void scan_watchpoint(vaddr_t pc) {
    WP *p;
    for (p = head; p != NULL; p = p->next) {
        bool success;
        word_t new_val = expr(p->expr, &success);
        if (p->old_val != new_val) {
            printf("\n\nHint watchpoint %d at address " FMT_WORD ", expr = %s\n", p->NO, pc, p->expr);
            printf("old value = " FMT_WORD "\nnew value = " FMT_WORD "\n", p->old_val, new_val);
            p->old_val = new_val;
            nemu_state.state = NEMU_STOP;
            return;
        }
    }
}