
struct us_queue
{
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	int size;
	int head;
	int tail;
	void * msgs[]; // sparse array
};

struct us_queue * us_queue_init(int size);
void us_queue_destroy(struct us_queue * q);
int us_queue_wait(struct us_queue * q);
int us_queue_enqueue(struct us_queue * q, void * item);
int us_queue_dequeue(struct us_queue * q, void ** item);
