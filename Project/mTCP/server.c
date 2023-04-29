#include "server.h"

/*----------------------------------------------------------------------------*/
static int 
SendUntilAvailable(struct thread_context *ctx, int sockid, struct server_vars *sv)
{
	int ret;
	int sent;
	int len;

	if (sv->done || !sv->rspheader_sent) {
		return 0;
	}

	sent = 0;
	ret = 1;
	while (ret > 0) {
		len = MIN(SNDBUF_SIZE, sv->fsize - sv->total_sent);
		if (len <= 0) {
			break;
		}
		ret = mtcp_write(ctx->mctx, sockid,  
				fcache[sv->fidx].file + sv->total_sent, len);
		if (ret < 0) {
			TRACE_APP("Connection closed with client.\n");
			break;
		}
		TRACE_APP("Socket %d: mtcp_write try: %d, ret: %d\n", sockid, len, ret);
		sent += ret;
		sv->total_sent += ret;
	}

	if (sv->total_sent >= fcache[sv->fidx].size) {
		struct mtcp_epoll_event ev;
		sv->done = TRUE;
		finished++;

		if (sv->keep_alive) {
			/* if keep-alive connection, wait for the incoming request */
			ev.events = MTCP_EPOLLIN;
			ev.data.sockid = sockid;
			mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

			CleanServerVariable(sv);
		} else {
			/* else, close connection */
			CloseConnection(ctx, sockid, sv);
		}
	}

	return sent;
}
/*----------------------------------------------------------------------------*/
static int 
HandleReadEvent(struct thread_context *ctx, int sockid, struct server_vars *sv)
{
	struct mtcp_epoll_event ev;
	char buf[HTTP_HEADER_LEN];
	char url[URL_LEN];
	char response[HTTP_HEADER_LEN];
	int scode;						// status code
	time_t t_now;
	char t_str[128];
	char keepalive_str[128];
	int rd;
	int i;
	int len;
	int sent;

	/* HTTP request handling */
	rd = mtcp_read(ctx->mctx, sockid, buf, HTTP_HEADER_LEN);
	if (rd <= 0) {
		return rd;
	}
	memcpy(sv->request + sv->recv_len, 
			(char *)buf, MIN(rd, HTTP_HEADER_LEN - sv->recv_len));
	sv->recv_len += rd;
	//sv->request[rd] = '\0';
	//fprintf(stderr, "HTTP Request: \n%s", request);
	sv->request_len = find_http_header(sv->request, sv->recv_len);
	if (sv->request_len <= 0) {
		TRACE_ERROR("Socket %d: Failed to parse HTTP request header.\n"
				"read bytes: %d, recv_len: %d, "
				"request_len: %d, strlen: %ld, request: \n%s\n", 
				sockid, rd, sv->recv_len, 
				sv->request_len, strlen(sv->request), sv->request);
		return rd;
	}

	http_get_url(sv->request, sv->request_len, url, URL_LEN);
	TRACE_APP("Socket %d URL: %s\n", sockid, url);
	sprintf(sv->fname, "%s%s", www_main, url);
	TRACE_APP("Socket %d File name: %s\n", sockid, sv->fname);

	sv->keep_alive = FALSE;
	if (http_header_str_val(sv->request, "Connection: ", 
				strlen("Connection: "), keepalive_str, 128)) {	
		if (strstr(keepalive_str, "Keep-Alive")) {
			sv->keep_alive = TRUE;
		} else if (strstr(keepalive_str, "Close")) {
			sv->keep_alive = FALSE;
		}
	}

	/* Find file in cache */
	scode = 404;
	for (i = 0; i < nfiles; i++) {
		if (strcmp(sv->fname, fcache[i].fullname) == 0) {
			sv->fsize = fcache[i].size;
			sv->fidx = i;
			scode = 200;
			break;
		}
	}
	TRACE_APP("Socket %d File size: %ld (%ldMB)\n", 
			sockid, sv->fsize, sv->fsize / 1024 / 1024);

	/* Response header handling */
	time(&t_now);
	strftime(t_str, 128, "%a, %d %b %Y %X GMT", gmtime(&t_now));
	if (sv->keep_alive)
		sprintf(keepalive_str, "Keep-Alive");
	else
		sprintf(keepalive_str, "Close");

	sprintf(response, "HTTP/1.1 %d %s\r\n"
			"Date: %s\r\n"
			"Server: Webserver on Middlebox TCP (Ubuntu)\r\n"
			"Content-Length: %ld\r\n"
			"Connection: %s\r\n\r\n", 
			scode, StatusCodeToString(scode), t_str, sv->fsize, keepalive_str);
	len = strlen(response);
	TRACE_APP("Socket %d HTTP Response: \n%s", sockid, response);
	sent = mtcp_write(ctx->mctx, sockid, response, len);
	TRACE_APP("Socket %d Sent response header: try: %d, sent: %d\n", 
			sockid, len, sent);
	assert(sent == len);
	sv->rspheader_sent = TRUE;

	ev.events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

	SendUntilAvailable(ctx, sockid, sv);

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
