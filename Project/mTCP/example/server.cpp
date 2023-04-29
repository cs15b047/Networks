#include "../common/server.h"
#include "utils.h"
#include <bits/stdc++.h>

using namespace std;

static int 
ServerRead(struct thread_context *ctx, int sockid)
{
	struct mtcp_epoll_event ev;
	char response[HTTP_HEADER_LEN];
	int rd;
	int len;
	int sent;
	int arr_len;

	rd = mtcp_read(ctx->mctx, sockid, (char *) &arr_len, sizeof(int));
	if (rd <= 0) {
		return rd;
	}

	printf("Received %d bytes\n", rd);
	printf("Received Array Length: %d\n", arr_len);

	vector<int> array(arr_len);

	int bytes_to_read = arr_len * sizeof(int);
	int recv_ptr = 0;
	while (bytes_to_read > 0) {
		rd = mtcp_read(ctx->mctx, sockid, (char *) (array.data() + recv_ptr), bytes_to_read);
		if (rd <= 0) {
			return rd;
		}

		bytes_to_read -= rd;
		recv_ptr += rd / sizeof(int);
	}

	printf("Received %d bytes\n", rd);
	printf("Received Array: ");
	for (int i = 0; i < arr_len; i++) {
		printf("%d ", array[i]);
	}
	printf("\n");

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
	int ret = ServerSetup(SERV_PORT);
	if (!ret) {
		TRACE_CONFIG("Failed to setup mtcp\n");
		exit(EXIT_FAILURE);
	}

	ServerStart();
	ServerStop();
	return 0;
}
