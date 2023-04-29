#include "server.h"

static int 
ServerRead(struct thread_context *ctx, int sockid)
{
	struct mtcp_epoll_event ev;
	char buf[HTTP_HEADER_LEN];
	char response[HTTP_HEADER_LEN];
	int rd;
	int len;
	int sent;

	rd = mtcp_read(ctx->mctx, sockid, buf, HTTP_HEADER_LEN);
	if (rd <= 0) {
		return rd;
	}

	printf("Received %d bytes\n", rd);
	printf("Received Message: %s\n", buf);

	strcpy(response, "hello from server");
	len = strlen(response);
	
	sent = mtcp_write(ctx->mctx, sockid, response, len);
	TRACE_APP("Socket %d: mtcp_write try: %d, ret: %d\n", sockid, len, sent);

	assert(sent == len);

	ev.events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);
	return rd;
}

int 
main(int argc, char **argv)
{	
	int ret = ServerSetup();
	if (!ret) {
		TRACE_CONFIG("Failed to setup mtcp\n");
		exit(EXIT_FAILURE);
	}

	ServerStart();
	ServerStop();
	return 0;
}
