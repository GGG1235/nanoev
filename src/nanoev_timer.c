#include "nanoev_internal.h"

/*----------------------------------------------------------------------------*/

struct nanoev_timer {
    NANOEV_EVENT_FILEDS
    struct nanoev_timeval timeout;
    nanoev_timer_callback callback;
    unsigned int min_heap_idx;
};
typedef struct nanoev_timer nanoev_timer;

static int min_heap_reserve(timer_min_heap *heap, unsigned int capacity);
static void min_heap_erase(timer_min_heap *heap, nanoev_timer *timer);
static void min_heap_shift_up(timer_min_heap *heap, unsigned int hole_index, nanoev_timer *timer);
static void min_heap_shift_down(timer_min_heap *heap, unsigned int hole_index, nanoev_timer *timer);

/*----------------------------------------------------------------------------*/

nanoev_event* timer_new(nanoev_loop *loop, void *userdata)
{
    nanoev_timer *timer;

    timer = (nanoev_timer*)mem_alloc(sizeof(nanoev_timer));
    if (!timer)
        return NULL;

    memset(timer, 0, sizeof(nanoev_timer));

    timer->type = nanoev_event_timer;
    timer->loop = loop;
    timer->userdata = userdata;
    timer->min_heap_idx = (unsigned int)-1;

    return (nanoev_event*)timer;
}

void timer_free(nanoev_event *event)
{
    nanoev_timer *timer = (nanoev_timer*)event;
    
    timer_min_heap *heap = get_loop_timers(timer->loop);
    min_heap_erase(heap, timer);

    mem_free(timer);
}

int nanoev_timer_add(
    nanoev_event *event,
    struct nanoev_timeval after,
    nanoev_timer_callback callback
    )
{
    nanoev_timer *timer = (nanoev_timer*)event;
    timer_min_heap *heap;
    int ret_code;

    ASSERT(timer);
    ASSERT(in_loop_thread(timer->loop));
    ASSERT(timer->min_heap_idx == (unsigned int)-1);
    ASSERT(callback);

    nanoev_loop_now(timer->loop, &timer->timeout);
    time_add(&timer->timeout, &after);
    timer->callback = callback;

    heap = get_loop_timers(timer->loop);
    ASSERT(heap);
    ret_code = min_heap_reserve(heap, heap->size + 1);
    if (ret_code != NANOEV_SUCCESS)
        return ret_code;

    min_heap_shift_up(heap, heap->size, timer);
    heap->size++;
    
    return NANOEV_SUCCESS;
}

int nanoev_timer_del(
    nanoev_event *event
    )
{
    nanoev_timer *timer = (nanoev_timer*)event;
    timer_min_heap *heap;

    ASSERT(timer);
    ASSERT(in_loop_thread(timer->loop));
    ASSERT(timer->min_heap_idx != (unsigned int)-1);

    heap = get_loop_timers(timer->loop);
    ASSERT(heap);
    min_heap_erase(heap, timer);

    return NANOEV_SUCCESS;
}

/*----------------------------------------------------------------------------*/

void timers_init(timer_min_heap *heap)
{
    ASSERT(heap);
    heap->events = NULL;
    heap->capacity = 0;
    heap->size = 0;
}

void timers_term(timer_min_heap *heap)
{
    ASSERT(heap);
    mem_free(heap->events);
}

unsigned int timers_timeout(timer_min_heap *heap, const struct nanoev_timeval *now)
{
    nanoev_timer *top;
    struct nanoev_timeval tv;

    ASSERT(heap && now);

    if (!heap->size)
        return 0xffffffff;  /* INFINITI */

    top = (nanoev_timer*)heap->events[0];
    tv = top->timeout;
    if (time_cmp(&tv, now) <= 0)
        return 0;

    time_sub(&tv, now);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;  /* convert to milliseconds */
}

void timers_process(timer_min_heap *heap, const struct nanoev_timeval *now)
{
    nanoev_timer *top;

    ASSERT(heap && now);

    while (heap->size) {
        top = (nanoev_timer*)heap->events[0];
        if (time_cmp(&top->timeout, now) > 0)
            break;

        top->callback((nanoev_event*)top);

        min_heap_erase(heap, top);
    }
}

/*----------------------------------------------------------------------------*/

static int __time_greater(nanoev_timer *t0, nanoev_timer *t1)
{
    int ret_code = time_cmp(&t0->timeout, &t1->timeout);
    return ret_code > 0 ? 1 : 0;
}

static int min_heap_reserve(timer_min_heap *heap, unsigned int capacity)
{
    ASSERT(heap);
    ASSERT(capacity);

    if (heap->capacity < capacity) {
        nanoev_event** events_new = (nanoev_event**)mem_realloc(
            heap->events, sizeof(nanoev_event*) * capacity);
        if (!events_new)
            return NANOEV_ERROR_OUT_OF_MEMORY;
        heap->events = events_new;
        heap->capacity = capacity;
    }

    return NANOEV_SUCCESS;
}

static void min_heap_erase(timer_min_heap *heap, nanoev_timer *timer)
{
    nanoev_timer *last;
    unsigned int parent;

    if (timer->min_heap_idx != (unsigned int)-1) {
        /* ������1 */
        heap->size--;
        
        /* �ٶ����һ���ڵ�����timer���ڵ�λ���ϣ�������timer����Ȼ������� */
        last = (nanoev_timer*)heap->events[heap->size];
        parent = (timer->min_heap_idx - 1) / 2;
        if (timer->min_heap_idx > 0 && __time_greater((nanoev_timer*)heap->events[parent], last))
            min_heap_shift_up(heap, timer->min_heap_idx, last);
        else
            min_heap_shift_down(heap, timer->min_heap_idx, last);

        /* ���timer��heap�е�index */
        timer->min_heap_idx = (unsigned int)-1;
    }
}

static void min_heap_shift_up(timer_min_heap *heap, unsigned int hole_index, nanoev_timer *timer)
{
    /* ѭ��������parent��timeout��timer��timeoutֵҪ���򽻻����ߵ�λ�� */
    unsigned int parent = (hole_index - 1) / 2;
    while (hole_index) {
        if (!__time_greater((nanoev_timer*)heap->events[parent], timer)) {
            break;
        }

        heap->events[hole_index] = heap->events[parent];
        ((nanoev_timer*)heap->events[hole_index])->min_heap_idx = hole_index;
        hole_index = parent;

        parent = (hole_index - 1) / 2;
    }

    /* ���ڵ�hole_index����timerӦ���ڵ�λ�� */
    heap->events[hole_index] = (nanoev_event*)timer;
    timer->min_heap_idx = hole_index;
}

static void min_heap_shift_down(timer_min_heap *heap, unsigned int hole_index, nanoev_timer *timer)
{
    unsigned int min_child = 2 * hole_index + 2;  /* �ȼ���right_child�ǽ�С���Ǹ� */
    while (min_child <= heap->size) {
        if (min_child == heap->size) {
            /* û��right_child��ֻ��left_child */
            min_child--;
        } else if (__time_greater((nanoev_timer*)heap->events[min_child], 
                    (nanoev_timer*)heap->events[min_child - 1])
                   ) {
            /* left_child��ֵ��right_childС����min_childָ��left_child */
            min_child--;
        }

        if (!__time_greater(timer, (nanoev_timer*)heap->events[min_child])) {
            break;
        }

        /* ��timer��min_childλ�û��� */
        heap->events[hole_index] = heap->events[min_child];
        ((nanoev_timer*)heap->events[hole_index])->min_heap_idx = min_child;
        hole_index = min_child;
        
        min_child = 2 * hole_index + 2;
    }

    /* ���ڵ�hole_index����timerӦ���ڵ�λ�� */
    heap->events[hole_index] = (nanoev_event*)timer;
    timer->min_heap_idx = hole_index;
}
