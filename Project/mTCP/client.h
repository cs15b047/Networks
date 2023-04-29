#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include <assert.h>
#include <limits.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>
#include "cpu.h"
#include "rss.h"
#include "http_parsing.h"
#include "netlib.h"
#include "debug.h"
#include "utils.h"

#define MAX_URL_LEN 128
#define FILE_LEN    128
#define FILE_IDX     10
#define MAX_FILE_LEN (FILE_LEN + FILE_IDX)
#define HTTP_HEADER_LEN 1024

#define IP_RANGE 1
#define MAX_IP_STR_LEN 16

#define BUF_SIZE (8*1024)

#define CALC_MD5SUM FALSE

#define TIMEVAL_TO_MSEC(t)		((t.tv_sec * 1000) + (t.tv_usec / 1000))
#define TIMEVAL_TO_USEC(t)		((t.tv_sec * 1000000) + (t.tv_usec))
#define TS_GT(a,b)				((int64_t)((a)-(b)) > 0)

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif



/*----------------------------------------------------------------------------*/
static pthread_t app_thread[MAX_CPUS];
static mctx_t g_mctx[MAX_CPUS];
static int done[MAX_CPUS];
/*----------------------------------------------------------------------------*/
static int num_cores;
static int core_limit;
/*----------------------------------------------------------------------------*/
static int fio = FALSE;
static char outfile[FILE_LEN + 1];
/*----------------------------------------------------------------------------*/
static char host[MAX_IP_STR_LEN + 1] = {'\0'};
static char url[MAX_URL_LEN + 1] = {'\0'};
static in_addr_t daddr;
static in_port_t dport;
static in_addr_t saddr;
/*----------------------------------------------------------------------------*/
static int total_flows;
static int flows[MAX_CPUS];
static int flowcnt = 0;
static int concurrency;
static int max_fds;
static uint64_t response_size = 0;
/*----------------------------------------------------------------------------*/
struct wget_stat
{
	uint64_t waits;
	uint64_t events;
	uint64_t connects;
	uint64_t reads;
	uint64_t writes;
	uint64_t completes;

	uint64_t errors;
	uint64_t timedout;

	uint64_t sum_resp_time;
	uint64_t max_resp_time;
};
/*----------------------------------------------------------------------------*/
struct thread_context
{
	int core;

	mctx_t mctx;
	int ep;
	struct wget_vars *wvars;

	int target;
	int started;
	int errors;
	int incompletes;
	int done;
	int pending;

	struct wget_stat stat;
};
typedef struct thread_context* thread_context_t;
/*----------------------------------------------------------------------------*/
struct wget_vars
{
	int request_sent;

	char response[HTTP_HEADER_LEN];
	int resp_len;
	int headerset;
	uint32_t header_len;
	uint64_t file_len;
	uint64_t recv;
	uint64_t write;

	struct timeval t_start;
	struct timeval t_end;
	
	int fd;
};
/*----------------------------------------------------------------------------*/
static struct thread_context *g_ctx[MAX_CPUS] = {0};
static struct wget_stat *g_stat[MAX_CPUS] = {0};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static inline int
HandleReadEvent(thread_context_t ctx, int sockid, struct wget_vars *wv);
/*----------------------------------------------------------------------------*/
static inline int 
SendHTTPRequest(thread_context_t ctx, int sockid, struct wget_vars *wv);

thread_context_t 
CreateContext(int core)
{
	thread_context_t ctx;

	ctx = (thread_context_t)calloc(1, sizeof(struct thread_context));
	if (!ctx) {
		perror("malloc");
		TRACE_ERROR("Failed to allocate memory for thread context.\n");
		return NULL;
	}
	ctx->core = core;

	ctx->mctx = mtcp_create_context(core);
	if (!ctx->mctx) {
		TRACE_ERROR("Failed to create mtcp context.\n");
		free(ctx);
		return NULL;
	}
	g_mctx[core] = ctx->mctx;

	return ctx;
}
/*----------------------------------------------------------------------------*/
void 
DestroyContext(thread_context_t ctx) 
{
	g_stat[ctx->core] = NULL;
	mtcp_destroy_context(ctx->mctx);
	free(ctx);
}
/*----------------------------------------------------------------------------*/
static inline int 
CreateConnection(thread_context_t ctx)
{
	mctx_t mctx = ctx->mctx;
	struct mtcp_epoll_event ev;
	struct sockaddr_in addr;
	int sockid;
	int ret;

	sockid = mtcp_socket(mctx, AF_INET, SOCK_STREAM, 0);
	if (sockid < 0) {
		TRACE_INFO("Failed to create socket!\n");
		return -1;
	}
	memset(&ctx->wvars[sockid], 0, sizeof(struct wget_vars));
	ret = mtcp_setsock_nonblock(mctx, sockid);
	if (ret < 0) {
		TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
		exit(-1);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = daddr;
	addr.sin_port = dport;
	
	ret = mtcp_connect(mctx, sockid, 
			(struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		if (errno != EINPROGRESS) {
			perror("mtcp_connect");
			mtcp_close(mctx, sockid);
			return -1;
		}
	}

	ctx->started++;
	ctx->pending++;
	ctx->stat.connects++;

	ev.events = MTCP_EPOLLOUT;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, sockid, &ev);

	return sockid;
}
/*----------------------------------------------------------------------------*/
static inline void 
CloseConnection(thread_context_t ctx, int sockid)
{
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
	mtcp_close(ctx->mctx, sockid);
	ctx->pending--;
	ctx->done++;
	assert(ctx->pending >= 0);
	while (ctx->pending < concurrency && ctx->started < ctx->target) {
		if (CreateConnection(ctx) < 0) {
			done[ctx->core] = TRUE;
			break;
		}
	}
}

/*----------------------------------------------------------------------------*/
static inline int 
DownloadComplete(thread_context_t ctx, int sockid, struct wget_vars *wv)
{
#ifdef APP
	mctx_t mctx = ctx->mctx;
#endif
	uint64_t tdiff;

	TRACE_APP("Socket %d File download complete!\n", sockid);
	gettimeofday(&wv->t_end, NULL);
	CloseConnection(ctx, sockid);
	ctx->stat.completes++;
	if (response_size == 0) {
		response_size = wv->recv;
		fprintf(stderr, "Response size set to %lu\n", response_size);
	} else {
		if (wv->recv != response_size) {
			fprintf(stderr, "Response size mismatch! mine: %lu, theirs: %lu\n", 
					wv->recv, response_size);
		}
	}
	tdiff = (wv->t_end.tv_sec - wv->t_start.tv_sec) * 1000000 + 
			(wv->t_end.tv_usec - wv->t_start.tv_usec);
	TRACE_APP("Socket %d Total received bytes: %lu (%luMB)\n", 
			sockid, wv->recv, wv->recv / 1000000);
	TRACE_APP("Socket %d Total spent time: %lu us\n", sockid, tdiff);
	if (tdiff > 0) {
		TRACE_APP("Socket %d Average bandwidth: %lf[MB/s]\n", 
				sockid, (double)wv->recv / tdiff);
	}
	ctx->stat.sum_resp_time += tdiff;
	if (tdiff > ctx->stat.max_resp_time)
		ctx->stat.max_resp_time = tdiff;

	if (fio && wv->fd > 0)
		close(wv->fd);

	return 0;
}

/*----------------------------------------------------------------------------*/
#if 0
void 
PrintStats()
{
#define LINE_LEN 2048
	char line[LINE_LEN];
	int total_trans;
	int i;

	total_trans = 0;
	line[0] = '\0';
	//sprintf(line, "Trans/s: ");
	for (i = 0; i < core_limit; i++) {
		//sprintf(line + strlen(line), "%6d  ", g_trans[i]);
		sprintf(line + strlen(line), "[CPU%2d] %7d trans/s  ", i, g_trans[i]);
		total_trans += g_trans[i];
		g_trans[i] = 0;
		if (i % 4 == 3)
			sprintf(line + strlen(line), "\n");
	}
	fprintf(stderr, "%s", line);
	fprintf(stderr, "[ ALL ] %7d trans/s\n", total_trans);
	//sprintf(line + strlen(line), "total: %6d", total_trans);
	//printf("%s\n", line);

	//fprintf(stderr, "Transactions/s: %d\n", total_trans);
	fflush(stderr);
}
#endif
/*----------------------------------------------------------------------------*/
static void 
PrintStats()
{
	struct wget_stat total = {0};
	struct wget_stat *st;
	uint64_t avg_resp_time;
	uint64_t total_resp_time = 0;
	int i;

	for (i = 0; i < core_limit; i++) {
		st = g_stat[i];

		if (st == NULL) continue;
		avg_resp_time = st->completes? st->sum_resp_time / st->completes : 0;
#if 0
		fprintf(stderr, "[CPU%2d] epoll_wait: %5lu, event: %7lu, "
				"connect: %7lu, read: %4lu MB, write: %4lu MB, "
				"completes: %7lu (resp_time avg: %4lu, max: %6lu us), "
				"errors: %2lu (timedout: %2lu)\n", 
				i, st->waits, st->events, st->connects, 
				st->reads / 1024 / 1024, st->writes / 1024 / 1024, 
				st->completes, avg_resp_time, st->max_resp_time, 
				st->errors, st->timedout);
#endif

		total.waits += st->waits;
		total.events += st->events;
		total.connects += st->connects;
		total.reads += st->reads;
		total.writes += st->writes;
		total.completes += st->completes;
		total_resp_time += avg_resp_time;
		if (st->max_resp_time > total.max_resp_time)
			total.max_resp_time = st->max_resp_time;
		total.errors += st->errors;
		total.timedout += st->timedout;

		memset(st, 0, sizeof(struct wget_stat));		
	}
	fprintf(stderr, "[ ALL ] connect: %7lu, read: %4lu MB, write: %4lu MB, "
			"completes: %7lu (resp_time avg: %4lu, max: %6lu us)\n", 
			total.connects, 
			total.reads / 1024 / 1024, total.writes / 1024 / 1024, 
			total.completes, total_resp_time / core_limit, total.max_resp_time);
#if 0
	fprintf(stderr, "[ ALL ] epoll_wait: %5lu, event: %7lu, "
			"connect: %7lu, read: %4lu MB, write: %4lu MB, "
			"completes: %7lu (resp_time avg: %4lu, max: %6lu us), "
			"errors: %2lu (timedout: %2lu)\n", 
			total.waits, total.events, total.connects, 
			total.reads / 1024 / 1024, total.writes / 1024 / 1024, 
			total.completes, total_resp_time / core_limit, total.max_resp_time, 
			total.errors, total.timedout);
#endif
}
/*----------------------------------------------------------------------------*/
void *
RunWgetMain(void *arg)
{
	thread_context_t ctx;
	mctx_t mctx;
	int core = *(int *)arg;
	struct in_addr daddr_in;
	int n, maxevents;
	int ep;
	struct mtcp_epoll_event *events;
	int nevents;
	struct wget_vars *wvars;
	int i;

	struct timeval cur_tv, prev_tv;
	//uint64_t cur_ts, prev_ts;

	mtcp_core_affinitize(core);

	ctx = CreateContext(core);
	if (!ctx) {
		return NULL;
	}
	mctx = ctx->mctx;
	g_ctx[core] = ctx;
	g_stat[core] = &ctx->stat;
	srand(time(NULL));

	mtcp_init_rss(mctx, saddr, IP_RANGE, daddr, dport);

	n = flows[core];
	if (n == 0) {
		TRACE_DBG("Application thread %d finished.\n", core);
		pthread_exit(NULL);
		return NULL;
	}
	ctx->target = n;

	daddr_in.s_addr = daddr;
	fprintf(stderr, "Thread %d handles %d flows. connecting to %s:%u\n", 
			core, n, inet_ntoa(daddr_in), ntohs(dport));

	/* Initialization */
	maxevents = max_fds * 3;
	ep = mtcp_epoll_create(mctx, maxevents);
	if (ep < 0) {
		TRACE_ERROR("Failed to create epoll struct!n");
		exit(EXIT_FAILURE);
	}
	events = (struct mtcp_epoll_event *)
			calloc(maxevents, sizeof(struct mtcp_epoll_event));
	if (!events) {
		TRACE_ERROR("Failed to allocate events!\n");
		exit(EXIT_FAILURE);
	}
	ctx->ep = ep;

	wvars = (struct wget_vars *)calloc(max_fds, sizeof(struct wget_vars));
	if (!wvars) {
		TRACE_ERROR("Failed to create wget variables!\n");
		exit(EXIT_FAILURE);
	}
	ctx->wvars = wvars;

	ctx->started = ctx->done = ctx->pending = 0;
	ctx->errors = ctx->incompletes = 0;

	gettimeofday(&cur_tv, NULL);
	//prev_ts = TIMEVAL_TO_USEC(cur_tv);
	prev_tv = cur_tv;

	while (!done[core]) {
		gettimeofday(&cur_tv, NULL);
		//cur_ts = TIMEVAL_TO_USEC(cur_tv);

		/* print statistics every second */
		if (core == 0 && cur_tv.tv_sec > prev_tv.tv_sec) {
		  	PrintStats();
			prev_tv = cur_tv;
		}

		while (ctx->pending < concurrency && ctx->started < ctx->target) {
			if (CreateConnection(ctx) < 0) {
				done[core] = TRUE;
				break;
			}
		}

		nevents = mtcp_epoll_wait(mctx, ep, events, maxevents, -1);
		ctx->stat.waits++;
	
		if (nevents < 0) {
			if (errno != EINTR) {
				TRACE_ERROR("mtcp_epoll_wait failed! ret: %d\n", nevents);
			}
			done[core] = TRUE;
			break;
		} else {
			ctx->stat.events += nevents;
		}

		for (i = 0; i < nevents; i++) {

			if (events[i].events & MTCP_EPOLLERR) {
				int err;
				socklen_t len = sizeof(err);

				TRACE_APP("[CPU %d] Error on socket %d\n", 
						core, events[i].data.sockid);
				ctx->stat.errors++;
				ctx->errors++;
				if (mtcp_getsockopt(mctx, events[i].data.sockid, 
							SOL_SOCKET, SO_ERROR, (void *)&err, &len) == 0) {
					if (err == ETIMEDOUT)
						ctx->stat.timedout++;
				}
				CloseConnection(ctx, events[i].data.sockid);

			} else if (events[i].events & MTCP_EPOLLIN) {
				HandleReadEvent(ctx, 
						events[i].data.sockid, &wvars[events[i].data.sockid]);

			} else if (events[i].events == MTCP_EPOLLOUT) {
				struct wget_vars *wv = &wvars[events[i].data.sockid];

				if (!wv->request_sent) {
					SendHTTPRequest(ctx, events[i].data.sockid, wv);
				} else {
					//TRACE_DBG("Request already sent.\n");
				}

			} else {
				TRACE_ERROR("Socket %d: event: %s\n", 
						events[i].data.sockid, EventToString(events[i].events));
				assert(0);
			}
		}

		if (ctx->done >= ctx->target) {
			fprintf(stdout, "[CPU %d] Completed %d connections, "
					"errors: %d incompletes: %d\n", 
					ctx->core, ctx->done, ctx->errors, ctx->incompletes);
			break;
		}
	}

	TRACE_INFO("Wget thread %d waiting for mtcp to be destroyed.\n", core);
	DestroyContext(ctx);

	TRACE_DBG("Wget thread %d finished.\n", core);
	pthread_exit(NULL);
	return NULL;
}
/*----------------------------------------------------------------------------*/
void 
SignalHandler(int signum)
{
	int i;

	for (i = 0; i < core_limit; i++) {
		done[i] = TRUE;
	}
}
/*----------------------------------------------------------------------------*/

struct mtcp_conf mcfg;
char *conf_file;
int cores[MAX_CPUS];
int flow_per_thread;
int flow_remainder_cnt;
int total_concurrency = 0;
int ret;
int i;
int process_cpu = -1;

int ParseArgs(int argc, char **argv) {
    if (argc != 2) {
		TRACE_CONFIG("Too few arguments!\n");
		TRACE_CONFIG("Usage: %s #flows\n", argv[0]);
		return FALSE;
	}

	daddr = inet_addr(SERV_ADDR);
	dport = htons(SERV_PORT);
	saddr = INADDR_ANY;

	total_flows = mystrtol(argv[2], 10);
	if (total_flows <= 0) {
		TRACE_CONFIG("Number of flows should be large than 0.\n");
		return FALSE;
	}

    return TRUE;
}

int ClientSetup() {
    num_cores = GetNumCPUs();
	core_limit = 8;
	concurrency = 100;
    conf_file = "./conf/client.conf";


    if (core_limit > num_cores) {
        TRACE_CONFIG("CPU limit should be smaller than the "
                    "number of CPUS: %d\n", num_cores);
        return FALSE;
    } else if (core_limit < 1) {
        TRACE_CONFIG("CPU limit should be greater than 0\n");
        return FALSE;
    }
    /** 
     * it is important that core limit is set 
     * before mtcp_init() is called. You can
     * not set core_limit after mtcp_init()
     */
    mtcp_getconf(&mcfg);
    mcfg.num_cores = core_limit;
    mtcp_setconf(&mcfg);


	if (total_flows < core_limit) {
		core_limit = total_flows;
	}

	/* per-core concurrency = total_concurrency / # cores */
	if (total_concurrency > 0)
		concurrency = total_concurrency / core_limit;

	/* set the max number of fds 3x larger than concurrency */
	max_fds = concurrency * 3;

	TRACE_CONFIG("Application configuration:\n");
	TRACE_CONFIG("URL: %s\n", url);
	TRACE_CONFIG("# of total_flows: %d\n", total_flows);
	TRACE_CONFIG("# of cores: %d\n", core_limit);
	TRACE_CONFIG("Concurrency: %d\n", total_concurrency);
	if (fio) {
		TRACE_CONFIG("Output file: %s\n", outfile);
	}

	if (conf_file == NULL) {
		TRACE_ERROR("mTCP configuration file is not set!\n");
		exit(EXIT_FAILURE);
	}
	
	ret = mtcp_init(conf_file);
	if (ret) {
		TRACE_ERROR("Failed to initialize mtcp.\n");
		exit(EXIT_FAILURE);
	}
	mtcp_getconf(&mcfg);
	mcfg.max_concurrency = max_fds;
	mcfg.max_num_buffers = max_fds;
	mtcp_setconf(&mcfg);

	mtcp_register_signal(SIGINT, SignalHandler);
    return TRUE;
}

void ClientStart() {
    flow_per_thread = total_flows / core_limit;
	flow_remainder_cnt = total_flows % core_limit;
	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		cores[i] = i;
		done[i] = FALSE;
		flows[i] = flow_per_thread;

		if (flow_remainder_cnt-- > 0)
			flows[i]++;

		if (flows[i] == 0)
			continue;

		if (pthread_create(&app_thread[i], 
					NULL, RunWgetMain, (void *)&cores[i])) {
			perror("pthread_create");
			TRACE_ERROR("Failed to create wget thread.\n");
			exit(-1);
		}

		if (process_cpu != -1)
			break;
	}

	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		pthread_join(app_thread[i], NULL);
		TRACE_INFO("Wget thread %d joined.\n", i);

		if (process_cpu != -1)
			break;
    }
}

void ClientStop(){
	mtcp_destroy();
}

