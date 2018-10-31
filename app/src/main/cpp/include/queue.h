#include <sys/types.h>
#include <pthread.h>

extern "C" {
#include "libavformat/avformat.h"
}

#ifndef PLAYER_QUEUE_H
#define PLAYER_QUEUE_H

// 队列最大值
#define QUEUE_MAX_SIZE 50

// 节点数据类型
typedef AVPacket* NodeElement;

// 节点
typedef struct _Node {
    // 数据
    NodeElement data;
    // 下一个
    struct _Node* next;
} Node;

// 队列
typedef struct _Queue {
    // 大小
    int size;
    // 队列头
    Node* head;
    // 队列尾
    Node* tail;
    // 是否阻塞
    bool is_block;
    // 线程锁
    pthread_mutex_t* mutex_id;
    // 线程条件变量
    pthread_cond_t* not_empty_condition;
    pthread_cond_t* not_full_condition;
} Queue;

/**
 * 初始化队列
 * @param queue
 */
void queue_init(Queue* queue);

/**
 * 销毁队列
 * @param queue
 */
void queue_destroy(Queue* queue);

/**
 * 判断是否为空
 * @param queue
 * @return
 */
bool queue_is_empty(Queue* queue);

/**
 * 判断是否已满
 * @param queue
 * @return
 */
bool queue_is_full(Queue* queue);

/**
 * 入队 (阻塞)
 * @param queue
 * @param element
 * @param tid
 * @param cid
 */
void queue_in(Queue* queue, NodeElement element);

/**
 * 出队 (阻塞)
 * @param queue
 * @param tid
 * @param cid
 * @return
 */
NodeElement queue_out(Queue* queue);

/**
 * 清空队列
 * @param queue
 */
void queue_clear(Queue* queue);

/**
 * 打断阻塞
 * @param queue
 */
void break_block(Queue* queue);

#endif //PLAYER_QUEUE_H
