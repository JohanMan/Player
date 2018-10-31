#include "queue.h"

/**
 * 初始化队列
 * @param queue
 */
void queue_init(Queue* queue) {
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;
    queue->is_block = true;
    queue->mutex_id = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(queue->mutex_id, NULL);
    queue->not_empty_condition = (pthread_cond_t*) malloc(sizeof(pthread_cond_t));
    pthread_cond_init(queue->not_empty_condition, NULL);
    queue->not_full_condition = (pthread_cond_t*) malloc(sizeof(pthread_cond_t));
    pthread_cond_init(queue->not_full_condition, NULL);
}

/**
 * 销毁队列
 * @param queue
 */
void queue_destroy(Queue* queue) {
    Node* node = queue->head;
    while (node != NULL) {
        queue->head = queue->head->next;
        free(node);
        node = queue->head;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->is_block = false;
    pthread_mutex_destroy(queue->mutex_id);
    pthread_cond_destroy(queue->not_empty_condition);
    pthread_cond_destroy(queue->not_full_condition);
    free(queue->mutex_id);
    free(queue->not_empty_condition);
    free(queue->not_full_condition);
}

/**
 * 判断是否为空
 * @param queue
 * @return
 */
bool queue_is_empty(Queue* queue) {
    return queue->size == 0;
}

/**
 * 判断是否已满
 * @param queue
 * @return
 */
bool queue_is_full(Queue* queue) {
    return queue->size == QUEUE_MAX_SIZE;
}

/**
 * 入队 (阻塞)
 * @param queue
 * @param element
 */
void queue_in(Queue* queue, NodeElement element) {
    pthread_mutex_lock(queue->mutex_id);
    while (queue_is_full(queue) && queue->is_block) {
        pthread_cond_wait(queue->not_full_condition, queue->mutex_id);
    }
    if (queue->size >= QUEUE_MAX_SIZE) {
        pthread_mutex_unlock(queue->mutex_id);
        return;
    }
    Node* node = (Node*) malloc(sizeof(Node));
    node->data = element;
    node->next = NULL;
    if (queue->tail == NULL) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }
    queue->size += 1;
    pthread_cond_signal(queue->not_empty_condition);
    pthread_mutex_unlock(queue->mutex_id);
}

/**
 * 出队 (阻塞)
 * @param queue
 * @return
 */
NodeElement queue_out(Queue* queue) {
    pthread_mutex_lock(queue->mutex_id);
    while (queue_is_empty(queue) && queue->is_block) {
        pthread_cond_wait(queue->not_empty_condition, queue->mutex_id);
    }
    if (queue->head == NULL) {
        pthread_mutex_unlock(queue->mutex_id);
        return NULL;
    }
    Node* node = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    NodeElement element = node->data;
    free(node);
    queue->size -= 1;
    pthread_cond_signal(queue->not_full_condition);
    pthread_mutex_unlock(queue->mutex_id);
    return element;
}

/**
 * 清空队列
 * @param queue
 */
void queue_clear(Queue* queue) {
    pthread_mutex_lock(queue->mutex_id);
    Node* node = queue->head;
    while (node != NULL) {
        queue->head = queue->head->next;
        free(node);
        node = queue->head;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->is_block = true;
    pthread_cond_signal(queue->not_full_condition);
    pthread_mutex_unlock(queue->mutex_id);
}

/**
 * 打断阻塞
 * @param queue
 */
void break_block(Queue* queue) {
    queue->is_block = false;
    pthread_cond_signal(queue->not_empty_condition);
    pthread_cond_signal(queue->not_full_condition);
}