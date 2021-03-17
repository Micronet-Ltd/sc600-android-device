/** Anthony Best <anthony.best@micronet-inc.com>
 *  Queue Implementation
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdbool.h>

#include <pthread.h>
#include <sched.h>
#include <string.h>

#include "queue.h"
#include "misc.h"

// Predicates
#define QUEUE_EMPTY(q)	((q)->tail == (q)->head)
#define QUEUE_FULL(q)	((((q)->tail+1) % (q)->size) == (q)->head)

#define QUEUE_INC_TAIL(q)	do { (q)->tail = ((q)->tail+1)%(q)->size; } while (0)
#define QUEUE_INC_HEAD(q)	do { (q)->head = ((q)->head+1)%(q)->size; } while (0)

struct us_queue * us_queue_init(int size)
{
	struct us_queue * q;

	q = malloc(sizeof(struct us_queue) + sizeof(void*)*size);
	if(!q) return q;

	q->head = q->tail = 0;
	q->size = size;

	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);

	return q;
}

void us_queue_destroy(struct us_queue * q)
{
	pthread_cond_destroy(&q->cond);
	pthread_mutex_destroy(&q->mutex);
	free(q);
}

int us_queue_wait(struct us_queue * q)
{
	int r;

	if(0 != (r = pthread_mutex_lock(&q->mutex)) ) err_abort(r, "lock mutex");

	while(q->tail == q->head)
		pthread_cond_wait(&q->cond, &q->mutex);

	if( 0 != (r = pthread_mutex_unlock(&q->mutex)) ) err_abort(r, "unlock mutex");

	return 0;
}

int us_queue_enqueue(struct us_queue * q, void * item)
{
	int r;
	int ret = -1;

	if( 0 != (r = pthread_mutex_lock(&q->mutex)) ) err_abort(r, "lock mutex");

	if( QUEUE_FULL(q) )
		goto out;

	q->msgs[q->tail] = item;

	QUEUE_INC_TAIL(q);

	pthread_cond_signal(&q->cond);

	ret = 0;

out:

	if( 0 != (r = pthread_mutex_unlock(&q->mutex)) ) err_abort(r, "unlock mutex");

	return ret;
}


int us_queue_dequeue(struct us_queue * q, void ** item)
{
	int r;
	int ret = -1;
	if( 0 != (r = pthread_mutex_lock(&q->mutex)) ) err_abort(r, "lock mutex");

	if(q->tail == q->head)
		goto out;

	*item = q->msgs[q->head];

	QUEUE_INC_HEAD(q);

	ret = 0;

out:
	if( 0 != (r = pthread_mutex_unlock(&q->mutex)) ) err_abort(r, "unlock mutex");

	return ret;
}



