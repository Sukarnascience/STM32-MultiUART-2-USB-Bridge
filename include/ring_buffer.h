#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_SIZE 256U
#define RING_MASK (RING_SIZE - 1U)

typedef struct {
    volatile uint8_t  buf[RING_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} ring_buf_t;

static inline void ring_init(ring_buf_t *rb)
{
    rb->head = 0; rb->tail = 0;
}
static inline bool ring_empty(const ring_buf_t *rb)
{
    return rb->head == rb->tail;
}
static inline bool ring_full(const ring_buf_t *rb)
{
    return ((rb->head - rb->tail) & 0xFFFFFFFFU) >= RING_SIZE;
}
static inline uint32_t ring_count(const ring_buf_t *rb)
{
    return (rb->head - rb->tail) & 0xFFFFFFFFU;
}
static inline bool ring_push(ring_buf_t *rb, uint8_t byte)
{
    if (ring_full(rb)) return false;
    rb->buf[rb->head & RING_MASK] = byte;
    __asm__ volatile ("" ::: "memory");
    rb->head++;
    return true;
}
static inline bool ring_pop(ring_buf_t *rb, uint8_t *byte)
{
    if (ring_empty(rb)) return false;
    *byte = rb->buf[rb->tail & RING_MASK];
    __asm__ volatile ("" ::: "memory");
    rb->tail++;
    return true;
}
static inline uint32_t ring_pop_bulk(ring_buf_t *rb, uint8_t *dst, uint32_t max)
{
    uint32_t n = ring_count(rb);
    if (n > max) n = max;
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = rb->buf[rb->tail & RING_MASK];
        rb->tail++;
    }
    return n;
}
static inline uint32_t ring_push_bulk(ring_buf_t *rb, const uint8_t *src, uint32_t len)
{
    uint32_t pushed = 0;
    while (pushed < len && !ring_full(rb)) {
        rb->buf[rb->head & RING_MASK] = src[pushed++];
        __asm__ volatile ("" ::: "memory");
        rb->head++;
    }
    return pushed;
}

#endif