#include "haclog/handler/haclog_file_time_rot_handler.h"
#include <stdlib.h>
#include <string.h>
#define HACLOG_HOLD_LOG_MACRO 1
#include "haclog/haclog.h"
#include <stdint.h>

#define MSG_CNT 2000
#define NUM_THREAD 8
#define ROUND 12
#define ROUND_INTERVAL 1000

typedef struct log_msg {
	uint64_t u64;
	uint32_t u32;
	int64_t i64;
	int32_t i32;
	char s[128];
} log_msg_t;

typedef struct thread_args {
	log_msg_t *arr;
	int arrlen;
	double *avg_elapsed_arr;
} thread_args_t;

void run_round(log_msg_t *arr, int arrlen, double *avg_elapsed)
{
	struct timespec ts1, ts2;
	timespec_get(&ts1, TIME_UTC);

	for (int i = 0; i < arrlen; i++) {
		log_msg_t *msg = arr + i;
		LOG_INFO("u64: %llu, i64: %lld, u32: %lu, i32: %ld, s: %s",
				 (unsigned long long)msg->u64, (long long)msg->i64,
				 (unsigned long)msg->u32, (long)msg->i32, msg->s);
	}

	timespec_get(&ts2, TIME_UTC);
	unsigned long elapsed =
		(ts2.tv_sec - ts1.tv_sec) * 1000000000 + ts2.tv_nsec - ts1.tv_nsec;
	*avg_elapsed = (double)elapsed / arrlen;
}

haclog_thread_ret_t run(void *args)
{
	thread_args_t *p = (thread_args_t *)args;

	log_msg_t *arr = p->arr;
	int arrlen = p->arrlen;
	double *avg_elapsed_arr = p->avg_elapsed_arr;

	haclog_thread_context_init();

	for (int r = 0; r < ROUND; r++) {
		run_round(arr, arrlen, avg_elapsed_arr + r);
		haclog_nsleep(ROUND_INTERVAL);
	}

	haclog_thread_context_cleanup();

	return 0;
}

void add_file_handler()
{
	static haclog_file_handler_t handler = {};
	if (haclog_file_handler_init(&handler,
								 "logs/multi_thread_benchmark.log", "w") != 0) {
		fprintf(stderr, "failed init file handler");
		exit(EXIT_FAILURE);
	}
	haclog_handler_set_level((haclog_handler_t *)&handler,
							 HACLOG_LEVEL_DEBUG);
	haclog_context_add_handler((haclog_handler_t *)&handler);
}

void add_file_rotate_handler()
{
	static haclog_file_rotate_handler_t handler = {};
	if (haclog_file_rotate_handler_init(&handler,
										"logs/multi_thread_benchmark.rot.log",
										MSG_CNT * NUM_THREAD, 5) != 0) {
		fprintf(stderr, "failed init file rotate handler");
		exit(EXIT_FAILURE);
	}
	haclog_handler_set_level((haclog_handler_t *)&handler,
							 HACLOG_LEVEL_DEBUG);
	haclog_context_add_handler((haclog_handler_t *)&handler);
}

void add_file_time_rot_handler()
{
	static haclog_file_time_rot_handler_t handler = {};
	if (haclog_file_time_rotate_handler_init(
			&handler, "logs/multi_thread_benchmark.time_rot.log",
			HACLOG_TIME_ROTATE_UNIT_DAY, 3, 0) != 0) {
		fprintf(stderr, "failed init file time rotate handler");
		exit(EXIT_FAILURE);
	}
	haclog_handler_set_level((haclog_handler_t *)&handler, HACLOG_LEVEL_DEBUG);
	haclog_context_add_handler((haclog_handler_t *)&handler);
}

int main()
{
	// prepare handler
	add_file_handler();
	// add_file_rotate_handler();
	// add_file_time_rot_handler();
	haclog_backend_run();

	// prepare datas
	srand(time(NULL));
	log_msg_t *arr = (log_msg_t *)malloc(sizeof(log_msg_t) * MSG_CNT);
	for (int i = 0; i < MSG_CNT; i++) {
		arr[i].u64 = (uint64_t)i;
		arr[i].u32 = (uint32_t)i;
		arr[i].i64 = (int64_t)i;
		arr[i].i32 = (int32_t)i;
		for (int j = 0; j < (int)sizeof(arr[i].s) - 1; j++) {
			arr[i].s[j] = (rand() % 26) + 'a';
		}
		arr[i].s[sizeof(arr[0].s) - 1] = '\0';
		memcpy(arr[i].s, "HACLOG123456", 12);
	}

	// run threads
	double *avg_elapsed_arr =
		(double *)malloc(sizeof(double) * NUM_THREAD * ROUND);
	haclog_thread_t *threads =
		(haclog_thread_t *)malloc(sizeof(haclog_thread_t) * NUM_THREAD);
	thread_args_t *args =
		(thread_args_t *)malloc(sizeof(thread_args_t) * NUM_THREAD);

	for (int i = 0; i < NUM_THREAD; ++i) {
		args[i].arr = arr;
		args[i].arrlen = MSG_CNT;
		args[i].avg_elapsed_arr = avg_elapsed_arr + i * ROUND;
	}
	for (int i = 0; i < NUM_THREAD; ++i) {
		haclog_thread_create(threads + i, run, &args[i]);
	}
	for (int i = 0; i < NUM_THREAD; ++i) {
		haclog_thread_join(threads + i);
	}

	// output result
	for (int i = 0; i < NUM_THREAD; i++) {
		printf("[%d] elapsed:", i);
		for (int r = 0; r < ROUND; ++r) {
			printf(" %8.3f |", avg_elapsed_arr[i * ROUND + r]);
		}
		printf("\n");
	}

	// cleanup
	free(args);
	free(threads);
	free(avg_elapsed_arr);

	free(arr);

	return 0;
}
