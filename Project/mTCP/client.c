#include "client.h"

static inline int 
ClientWrite(thread_context_t ctx, int sockid)
{
	char request[HTTP_HEADER_LEN];
	struct mtcp_epoll_event ev;
	int wr;
	int len;

	strcpy(request, "hello world");
	len = strlen(request);

	wr = mtcp_write(ctx->mctx, sockid, request, len);
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}

	ctx->stat.writes += wr;
	TRACE_APP("Socket %d HTTP Request of %d bytes. sent.\n", sockid, wr);

	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

	return 0;
}

static inline int
ClientRead(thread_context_t ctx, int sockid)
{
	mctx_t mctx = ctx->mctx;
	char buf[BUF_SIZE];
	int rd;

	rd = 1;
	while (rd > 0) {
		rd = mtcp_read(mctx, sockid, buf, BUF_SIZE);
		if (rd <= 0)
			break;

		printf("Received %d bytes\n", rd);
		printf("Received Message: %s\n", buf);
		ctx->stat.reads += rd;
		TRACE_APP("Socket %d received %d bytes.\n", sockid, rd);
	}

	if (rd == 0) {
		TRACE_DBG("Socket %d connection closed with server.\n", sockid);		
		ctx->stat.errors++;
		ctx->incompletes++;
		CloseConnection(ctx, sockid);

	} else if (rd < 0) {
		if (errno != EAGAIN) {
			TRACE_DBG("Socket %d: mtcp_read() error %s\n", 
					sockid, strerror(errno));
			ctx->stat.errors++;
			ctx->errors++;
			CloseConnection(ctx, sockid);
		}
	}

	return 0;
}

int 
main(int argc, char **argv)
{    
    ret = ParseArgs(argc, argv);
    if (!ret) {
        TRACE_ERROR("Failed to parse arguments.\n");
        exit(EXIT_FAILURE);
    }

    ret = ClientSetup();
    if (!ret) {
        TRACE_ERROR("Failed to setup client.\n");
        exit(EXIT_FAILURE);
    }
	
    ClientStart();
    ClientStop();
	return 0;
}