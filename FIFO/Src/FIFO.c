//
// Created by YinMi on 2026/4/3.
//

#include "FIFO.h"

void FIFO_Init(Fifo_Type *fifo)
{
    fifo->head = 0;
    fifo->tail = 0;
}

bool Fifo_Is_Empty(Fifo_Type *fifo)
{
    return fifo->head == fifo->tail;
}

bool Fifo_Is_Full(Fifo_Type *fifo)
{
    return ((fifo->head + 1) % FIFO_SIZE) == fifo->tail;
}

bool Fifo_Push(Fifo_Type *fifo, uint8_t data)
{
    uint16_t next = (fifo->head + 1) % FIFO_SIZE;

    if (next == fifo->tail)
    {
        return false; // 满了
    }

    fifo->buffer[fifo->head] = data;
    fifo->head = next;
    return true;
}

bool Fifo_Pop(Fifo_Type *fifo, uint8_t *data)
{
    if (fifo->head == fifo->tail)
    {
        return false; // 空
    }

    *data = fifo->buffer[fifo->tail];
    fifo->tail = (fifo->tail + 1) % FIFO_SIZE;
    return true;
}