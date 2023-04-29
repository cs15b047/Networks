#include "client.h"
#define PORT 80
/*----------------------------------------------------------------------------*/
static inline int 
SendHTTPRequest(thread_context_t ctx, int sockid, struct wget_vars *wv)
{
	char request[HTTP_HEADER_LEN];
	struct mtcp_epoll_event ev;
	int wr;
	int len;

	wv->headerset = FALSE;
	wv->recv = 0;
	wv->header_len = wv->file_len = 0;

	snprintf(request, HTTP_HEADER_LEN, "GET %s HTTP/1.0\r\n"
			"User-Agent: Wget/1.12 (linux-gnu)\r\n"
			"Accept: */*\r\n"
			"Host: %s\r\n"
//			"Connection: Keep-Alive\r\n\r\n", 
			"Connection: Close\r\n\r\n", 
			url, host);
	len = strlen(request);

	wr = mtcp_write(ctx->mctx, sockid, request, len);
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}
	ctx->stat.writes += wr;
	TRACE_APP("Socket %d HTTP Request of %d bytes. sent.\n", sockid, wr);
	wv->request_sent = TRUE;

	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

	gettimeofday(&wv->t_start, NULL);

	char fname[MAX_FILE_LEN + 1];
	if (fio) {
		snprintf(fname, MAX_FILE_LEN, "%s.%d", outfile, flowcnt++);
		wv->fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (wv->fd < 0) {
			TRACE_APP("Failed to open file descriptor for %s\n", fname);
			exit(1);
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static inline int
HandleReadEvent(thread_context_t ctx, int sockid, struct wget_vars *wv)
{
	mctx_t mctx = ctx->mctx;
	char buf[BUF_SIZE];
	char *pbuf;
	int rd, copy_len;

	rd = 1;
	while (rd > 0) {
		rd = mtcp_read(mctx, sockid, buf, BUF_SIZE);
		if (rd <= 0)
			break;
		ctx->stat.reads += rd;

		TRACE_APP("Socket %d: mtcp_read ret: %d, total_recv: %lu, "
				"header_set: %d, header_len: %u, file_len: %lu\n", 
				sockid, rd, wv->recv + rd, 
				wv->headerset, wv->header_len, wv->file_len);

		pbuf = buf;
		if (!wv->headerset) {
			copy_len = MIN(rd, HTTP_HEADER_LEN - wv->resp_len);
			memcpy(wv->response + wv->resp_len, buf, copy_len);
			wv->resp_len += copy_len;
			wv->header_len = find_http_header(wv->response, wv->resp_len);
			if (wv->header_len > 0) {
				wv->response[wv->header_len] = '\0';
				wv->file_len = http_header_long_val(wv->response, 
						CONTENT_LENGTH_HDR, sizeof(CONTENT_LENGTH_HDR) - 1);
				if (wv->file_len < 0) {
					/* failed to find the Content-Length field */
					wv->recv += rd;
					rd = 0;
					CloseConnection(ctx, sockid);
					return 0;
				}

				TRACE_APP("Socket %d Parsed response header. "
						"Header length: %u, File length: %lu (%luMB)\n", 
						sockid, wv->header_len, 
						wv->file_len, wv->file_len / 1024 / 1024);
				wv->headerset = TRUE;
				wv->recv += (rd - (wv->resp_len - wv->header_len));
				
				pbuf += (rd - (wv->resp_len - wv->header_len));
				rd = (wv->resp_len - wv->header_len);
				//printf("Successfully parse header.\n");
				//fflush(stdout);

			} else {
				/* failed to parse response header */
#if 0
				printf("[CPU %d] Socket %d Failed to parse response header."
						" Data: \n%s\n", ctx->core, sockid, wv->response);
				fflush(stdout);
#endif
				wv->recv += rd;
				rd = 0;
				ctx->stat.errors++;
				ctx->errors++;
				CloseConnection(ctx, sockid);
				return 0;
			}
			//pbuf += wv->header_len;
			//wv->recv += wv->header_len;
			//rd -= wv->header_len;
		}
		wv->recv += rd;
		
		if (fio && wv->fd > 0) {
			int wr = 0;
			while (wr < rd) {
				int _wr = write(wv->fd, pbuf + wr, rd - wr);
				assert (_wr == rd - wr);
				 if (_wr < 0) {
					 perror("write");
					 TRACE_ERROR("Failed to write.\n");
					 assert(0);
					 break;
				 }
				 wr += _wr;	
				 wv->write += _wr;
			}
		}
		
		if (wv->header_len && (wv->recv >= wv->header_len + wv->file_len)) {
			break;
		}
	}

	if (rd > 0) {
		if (wv->header_len && (wv->recv >= wv->header_len + wv->file_len)) {
			TRACE_APP("Socket %d Done Write: "
					"header: %u file: %lu recv: %lu write: %lu\n", 
					sockid, wv->header_len, wv->file_len, 
					wv->recv - wv->header_len, wv->write);
			DownloadComplete(ctx, sockid, wv);

			return 0;
		}

	} else if (rd == 0) {
		/* connection closed by remote host */
		TRACE_DBG("Socket %d connection closed with server.\n", sockid);

		if (wv->header_len && (wv->recv >= wv->header_len + wv->file_len)) {
			DownloadComplete(ctx, sockid, wv);
		} else {
			ctx->stat.errors++;
			ctx->incompletes++;
			CloseConnection(ctx, sockid);
		}

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