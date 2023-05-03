#include <common/client.h>
#include "utils.h"
#include <bits/stdc++.h>

using namespace std;

vector<int> create_random_array(int arr_len) {
	vector<int> v;
	for (int i = 0; i < arr_len; i++) {
		v.push_back(rand() % 100);
	}

	return v;
}

static inline int 
ClientWrite(thread_context_t ctx, int sockid)
{
	struct mtcp_epoll_event ev;
	int wr;
	int len;
	int arr_len = 100;
	vector<int> array = create_random_array(arr_len);

	wr = mtcp_write(ctx->mctx, sockid, (char*) &arr_len, sizeof(int));
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}

	wr = mtcp_write(ctx->mctx, sockid, (char*) array.data(), arr_len * sizeof(int));
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}
	

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
		TRACE_APP("Socket %d received %d bytes.\n", sockid, rd);
	}

	if (rd == 0) {
		TRACE_DBG("Socket %d connection closed with server.\n", sockid);		
		ctx->incompletes++;
		CloseConnection(ctx, sockid);

	} else if (rd < 0) {
		if (errno != EAGAIN) {
			TRACE_DBG("Socket %d: mtcp_read() error %s\n", 
					sockid, strerror(errno));
			ctx->errors++;
			CloseConnection(ctx, sockid);
		}
	}

	return 0;
}

int 
main(int argc, char **argv)
{    
	char *host = const_cast<char*>(SERV_ADDR);
    ret = ClientSetup(host, SERV_PORT);
    if (!ret) {
        TRACE_ERROR("Failed to setup client.\n");
        exit(EXIT_FAILURE);
    }
	
    ClientStart();
    ClientStop();
	return 0;
}