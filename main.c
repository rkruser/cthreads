#include <stdio.h> // Printf
#include <stdlib.h> // Basic stuff?
#include <unistd.h> // Sleep
#include <pthread.h> // Threads
#include <assert.h>
//#include <limits.h>

// Use calloc here
// pointer = calloc(count, sizeof(thing))

typedef struct StringSectionStruct {
	size_t size;
	char* string;
	int canFree;
	//int canFree; //Tells whether this struct owns its memory allocation
} StringSection;

StringSection func0(StringSection s1, StringSection s2) {
//	printf("In function 0\n");
	if (s1.size > 0) {
		s1.string[0] = 'a';
	}
	return s1;
}

StringSection func1(StringSection s1, StringSection s2) {
//	printf("In function 1\n");
	if (s1.size > 0) {
		s1.string[0] = 'b';
	}
	return s1;
}

StringSection func2(StringSection s1, StringSection s2) {
//	printf("In function 2\n");
	if (s1.size > 0 && s2.size > 0) {
		s1.string[0] = s2.string[0];
	}
	return s1;
}


const int NUM_FUNCTIONS = 3;

StringSection (*getFunction(int n))(StringSection, StringSection) {
	switch(n) {
		case 0:
			return func0;
			break;
		case 1:
			return func1;
			break;
		case 2:
			return func2;
			break;
		default:
			return func0;
	}
}

// If location is NULL, a new string is created of the right size and randomly populated
// If location is not null, n is ignored
StringSection newStringSection(size_t size, char* location, unsigned int n) {
	StringSection s;
	s.size = size;
	if (size == 0) {
		s.string = NULL;
		s.canFree = 0;
	}
	else {
		if (location == NULL) {
			s.string = malloc(size*sizeof(char));
			s.canFree = 1;
			size_t i;
			for (i=0; i<size; ++i) {
				s.string[i] = 'a'+(n*i+1727482737)%26; //semi-random
			}
		}
		else {
			s.string = location;
			s.canFree = 0;
		}
	}
	return s;
}

void destroyStringSection(StringSection* s) {
	if (s->canFree) {
		free(s->string);
	}
	s->size = 0;
	s->string = NULL;
	s->canFree = 0;
}

// Define a bunch of string functions here
// Put them in a switch statement in order to easily get random functions


typedef struct TaskNodeStruct {
	StringSection (*func)(StringSection, StringSection); //Change function type later, maybe
	StringSection arg; //Initial arg for the first function, modifier arg for subsequent
	struct TaskNodeStruct *prev;
	struct TaskNodeStruct *next;
} TaskNode;

TaskNode* create_random_task(StringSection initial_arg) {
	// Get random integer 1 to 10
	// Malloc an array of that many task nodes
	// Go through array, set variables appropriately (pick random functions?)
	// Return root node
//	printf("In create_random_task\n");
	size_t numtasks = rand()%10 + 1; //Between 1 and 10 in size
	TaskNode* root = malloc(numtasks*sizeof(TaskNode));
	
	// Initialize first node
	root[0].prev = NULL;
	root[0].arg = initial_arg;
	if (numtasks > 1) {
		root[0].next = &root[1];
	}
	root[0].func = getFunction(rand()%NUM_FUNCTIONS);

	size_t i;
	for (i=1; i<numtasks-1; ++i) {
		root[i].next = &root[i+1];
		root[i].prev = &root[i-1];
		root[i].func = getFunction(rand()%NUM_FUNCTIONS);
		root[i].arg = newStringSection(100, NULL, rand());
	}

	// Initialize last node
	if (numtasks > 1) {
		root[numtasks-1].prev = &root[numtasks-2];
	}
	root[numtasks-1].next = NULL;
	root[numtasks-1].func = getFunction(rand()%NUM_FUNCTIONS);
	root[numtasks-1].arg = newStringSection(100,NULL,rand());

	return root; //Why don't you warn me about this, compiler?
}

void execute_task(TaskNode* task) {
	if (task == NULL) return;
	StringSection dummy = newStringSection(0, NULL, 0);
	StringSection result = task->func(task->arg, dummy);
	task = task->next;
	while (task != NULL) {
		result = task->func(result, task->arg); //Where is result freed if new memory?
		task = task->next; 
	}
}

void destroy_task(TaskNode* root) {
	// Assuming root is the true root node,
	// free the array
	TaskNode* iter = root;
	while(iter != NULL) {
		destroyStringSection(&iter->arg);
		iter = iter->next;
	}
	free(root);
}

// *****************************************
typedef struct TaskQueueStruct {
	size_t current_size;
	size_t max_size;
	size_t head;
	size_t tail;
	TaskNode **tasks; //Array of pointers to task root nodes
	
	// Thread stuff
	pthread_mutex_t lock;
	pthread_cond_t free_space_exists;
	pthread_cond_t data_is_available;
} TaskQueue;

TaskQueue* create_task_queue(size_t max_size) {
	// Allocate an array of max_size size
	// Allocate the task queue struct itself
	// Populate the struct
	// return pointer to the allocated struct
	
	TaskQueue* queue = malloc(sizeof(TaskQueue));
	queue->current_size = 0;
	queue->max_size = max_size;
	queue->tasks = calloc(max_size, sizeof(TaskNode*));
	queue->head = 0;
	queue->tail = 0;
//	queue->lock = PTHREAD_MUTEX_INITIALIZER;
//	queue->free_space_exists = PTHREAD_COND_INITIALIZER;
//	queue->data_is_available = PTHREAD_COND_INITIALIZER;
	pthread_mutex_init(&queue->lock, NULL);
	pthread_cond_init(&queue->free_space_exists, NULL);
	pthread_cond_init(&queue->data_is_available, NULL);
	return queue;
}

void destroy_task_queue(TaskQueue *queue) {
	// Free the tasknode pointer array
	// Then free the struct itself
	free(queue->tasks);
	free(queue);
}

void task_push(TaskQueue *queue, TaskNode *task) {
	// Acquire mutex
	// Wait for condition variable that space is available (current_size < max_size)
	// Add task to end of queue and increment current_size 
	// Signal that data is available
	// Release mutex
	pthread_mutex_lock(&queue->lock);
	while (queue->current_size == queue->max_size) {
		pthread_cond_wait(&queue->free_space_exists, &queue->lock);
	}
	*(queue->tasks + queue->tail) = task;
	queue->tail = (queue->tail+1)%queue->max_size;
	queue->current_size++;

	pthread_cond_signal(&queue->data_is_available);
	pthread_mutex_unlock(&queue->lock);
}

TaskNode* task_pop(TaskQueue *queue) {
	// Acquire mutex
	// Wait for condition variable that data is available
	// Pop task
	// Signal that space is available
	// Release mutex
	// Return the task
	pthread_mutex_lock(&queue->lock);
	while (queue->current_size == 0) {
		pthread_cond_wait(&queue->data_is_available, &queue->lock);
	}

	TaskNode** headpointer = queue->tasks+queue->head;
	TaskNode* task = *headpointer;
	*headpointer = NULL;
	queue->head = (queue->head+1)%queue->max_size;
	queue->current_size--;

	pthread_cond_signal(&queue->free_space_exists);
	pthread_mutex_unlock(&queue->lock);

	return task;
}

// ****************************************

typedef struct ThreadArgumentStruct {
	TaskQueue *queue;
	int thread_number;
	int use_sleep;
	int verbose;
} ThreadArguments;

typedef struct ThreadInfoStruct {
	pthread_t thread_id; //thread id 
	//pthread_attr_t thread_attr;
	//void* (*func)(void*); //thread function
	ThreadArguments args; //arguments to initialize thread function, like stream
} ThreadInfo;

// *****************************************




void* ThreadFunction(void* arg) {
	ThreadArguments *thread_args = arg;
	TaskQueue *queue = thread_args->queue;	
	int thread_number = thread_args->thread_number;
	int use_sleep = thread_args->use_sleep;
	int verbose = thread_args->verbose;

	// Loop endlessly:
	//   Pop a task off the queue
	//   If the task is Null, terminate thread
	//   Otherwise execute the task
	//   Free the memory associated with the task?
	
	int numLoops = 0;
	while (1) {
		numLoops++;
		TaskNode* task = task_pop(queue);
		if (task == NULL) { //task or task->func?
			if (verbose) {
				printf("Exiting thread %i\n", thread_number);
			}
			break;
		}
		execute_task(task);		
		destroy_task(task);
		if (verbose) {
			printf("Thread %i task %i\n", thread_number, numLoops);
		}
		if (use_sleep) {
			sleep(2*(float)rand() / RAND_MAX);
		}
	}
	return NULL;
}





// **************************************************
typedef struct ThreadPoolStruct {
	size_t num_threads;
	ThreadInfo *all_threads;
} ThreadPool;


ThreadPool* create_thread_pool(size_t num_threads, int use_sleep, int verbose) {
// Create a thread pool all with the same function running in each thread
// Each with a different task queue
	ThreadPool* pool = malloc(sizeof(ThreadPool));
	pool->num_threads = num_threads;
	pool->all_threads = calloc(num_threads, sizeof(ThreadInfo));
	size_t i;
	for (i=0; i<num_threads; ++i) {
		pool->all_threads[i].args.queue = create_task_queue(1000);
		pool->all_threads[i].args.thread_number = i;
		pool->all_threads[i].args.use_sleep = use_sleep;
		pool->all_threads[i].args.verbose = verbose;
		pthread_create(&pool->all_threads[i].thread_id, NULL, ThreadFunction, &pool->all_threads[i].args); //Need ampersand before ThreadFunction?
	}
	return pool;
}

void destroy_thread_pool(ThreadPool* pool) {
	// Call after joining threads or cancelling threads
	// Destroy all task queues
	// Then free the thread info array
	size_t i;

	// Send termination signal
	for (i=0; i<pool->num_threads; ++i) {
		task_push(pool->all_threads[i].args.queue, NULL);
	}

	// Join all threads
	for (i=0; i<pool->num_threads; ++i) {
		pthread_join(pool->all_threads[i].thread_id, NULL);
	}

	// Destroy all queues
	for (i=0; i<pool->num_threads; ++i) {
		destroy_task_queue(pool->all_threads[i].args.queue);
	}

	// Free the pool itself
	free(pool->all_threads);
	free(pool);
}

// **************************************************


int main(int argc, char** argv) {
//	printf("Beginning of main\n");
//	const size_t NUM_THREADS = 10;
	size_t NUM_THREADS = 10;
	size_t NUM_TASKS = 100;
	int use_sleep = 0;
	int verbose = 0;
	int c;
	while ((c = getopt(argc, argv, "r:t:sv")) != -1) {
		switch(c) {
			case 'r':
				NUM_THREADS = (size_t) atoi(optarg);
				break;
			case 't':
				NUM_TASKS = (size_t) atoi(optarg);
				break;
			case 's':
				use_sleep = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			default:
				printf("Unknown option\n");
		}
	}
	printf("Threads: %ld, Tasks: %ld\n", NUM_THREADS, NUM_TASKS);
	
//	printf("Before thread pool\n");
	ThreadPool* pool = create_thread_pool(NUM_THREADS, use_sleep, verbose);
//	printf("After thread pool\n");

//	printf("Before main string\n");
	StringSection mainString = newStringSection(NUM_THREADS*100, NULL, 1234782);
//	printf("After main string\n");

//	printf("Before string portions loop\n");
	StringSection* stringPortions = malloc(NUM_THREADS*sizeof(StringSection));
	size_t i;
	for (i=0; i<NUM_THREADS; ++i) {
		stringPortions[i] = newStringSection(100, mainString.string+i*100, 0);
	}
//	printf("After stringPortions loop\n");
	
//	printf("Before task loop\n");
	for (i=0; i<NUM_TASKS; ++i){
//		printf("Before queue set\n");
		//assert(pool != NULL);
		//assert(pool->all_threads != NULL);
		TaskQueue *queue = pool->all_threads[i%(pool->num_threads)].args.queue;
//		printf("After queue set, before create random task\n");
		TaskNode *root = create_random_task(stringPortions[i%(pool->num_threads)]);
//		printf("After create random task\n");
//		printf("Before task push\n");
		task_push(queue, root);
//		printf("After task push\n");
	}
//	printf("After task loop\n");

	destroy_thread_pool(pool);
	free(stringPortions);
	destroyStringSection(&mainString);

	return 0;
}
