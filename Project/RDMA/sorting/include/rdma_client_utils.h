#ifndef RDMA_CLIENT_UTILS_H
#define RDMA_CLIENT_UTILS_H

#include "rdma_common.h"

using namespace std;

struct Connection {
	struct rdma_event_channel *cm_event_channel_client = NULL;
	struct rdma_cm_id *cm_client_id = NULL;
	struct ibv_pd *pd = NULL;
	struct ibv_comp_channel *io_completion_channel = NULL;
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_qp *client_qp = NULL;
	struct ibv_cq *client_cq = NULL;
	struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;

	/* These are memory buffers related resources */
	struct ibv_mr *client_metadata_mr = NULL, *server_buffer_mr = NULL, *server_metadata_mr = NULL;
	struct ibv_mr *client_src_mr = NULL, *client_dst_mr = NULL;
	struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;
	struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
	struct ibv_sge client_send_sge, server_recv_sge;
	/* Source and Destination buffers, where RDMA operations source and sink */ 
};

/* This function prepares client side connection resources for an RDMA connection */
static int client_prepare_connection(struct sockaddr_in *s_addr, struct Connection *conn_state)
{
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	/*  Open a channel used to report asynchronous communication event */
	conn_state->cm_event_channel_client = rdma_create_event_channel();
	if (!conn_state->cm_event_channel_client) {
		// rdma_error("Creating cm event channel failed, errno: %d \n", -errno);
		return -errno;
	}
	debug("RDMA CM event channel is created at : %p \n", cm_event_channel_client);
	/* rdma_cm_id is the connection identifier (like socket) which is used 
	 * to define an RDMA connection. 
	 */
	ret = rdma_create_id(conn_state->cm_event_channel_client, &conn_state->cm_client_id, 
			NULL,
			RDMA_PS_TCP);
	if (ret) {
		// rdma_error("Creating cm id failed with errno: %d \n", -errno); 
		return -errno;
	}
	/* Resolve destination and optional source addresses from IP addresses  to
	 * an RDMA address.  If successful, the specified rdma_cm_id will be bound
	 * to a local device. */
	ret = rdma_resolve_addr(conn_state->cm_client_id, NULL, (struct sockaddr*) s_addr, 2000);
	if (ret) {
		// rdma_error("Failed to resolve address, errno: %d \n", -errno);
		return -errno;
	}
	debug("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n");
	ret  = process_rdma_cm_event(conn_state->cm_event_channel_client, 
			RDMA_CM_EVENT_ADDR_RESOLVED,
			&cm_event);
	if (ret) {
		// rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}
	/* we ack the event */
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		// rdma_error("Failed to acknowledge the CM event, errno: %d\n", -errno);
		return -errno;
	}
	debug("RDMA address is resolved \n");

	 /* Resolves an RDMA route to the destination address in order to 
	  * establish a connection */
	ret = rdma_resolve_route(conn_state->cm_client_id, 2000);
	if (ret) {
		// rdma_error("Failed to resolve route, erno: %d \n", -errno);
	       return -errno;
	}
	debug("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
	ret = process_rdma_cm_event(conn_state->cm_event_channel_client, 
			RDMA_CM_EVENT_ROUTE_RESOLVED,
			&cm_event);
	if (ret) {
		// rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}
	/* we ack the event */
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		// rdma_error("Failed to acknowledge the CM event, errno: %d \n", -errno);
		return -errno;
	}
	printf("Trying to connect to server at : %s port: %d \n", 
			inet_ntoa(s_addr->sin_addr),
			ntohs(s_addr->sin_port));
	/* Protection Domain (PD) is similar to a "process abstraction" 
	 * in the operating system. All resources are tied to a particular PD. 
	 * And accessing recourses across PD will result in a protection fault.
	 */
	conn_state->pd = ibv_alloc_pd(conn_state->cm_client_id->verbs);
	if (!conn_state->pd) {
		// rdma_error("Failed to alloc pd, errno: %d \n", -errno);
		return -errno;
	}
	debug("pd allocated at %p \n", pd);
	/* Now we need a completion channel, were the I/O completion 
	 * notifications are sent. Remember, this is different from connection 
	 * management (CM) event notifications. 
	 * A completion channel is also tied to an RDMA device, hence we will 
	 * use cm_client_id->verbs. 
	 */
	conn_state->io_completion_channel = ibv_create_comp_channel(conn_state->cm_client_id->verbs);
	if (!conn_state->io_completion_channel) {
		// rdma_error("Failed to create IO completion event channel, errno: %d\n",-errno);
	return -errno;
	}
	debug("completion event channel created at : %p \n", io_completion_channel);
	/* Now we create a completion queue (CQ) where actual I/O 
	 * completion metadata is placed. The metadata is packed into a structure 
	 * called struct ibv_wc (wc = work completion). ibv_wc has detailed 
	 * information about the work completion. An I/O request in RDMA world 
	 * is called "work" ;) 
	 */
	conn_state->client_cq = ibv_create_cq(conn_state->cm_client_id->verbs /* which device*/, 
			CQ_CAPACITY /* maximum capacity*/, 
			NULL /* user context, not used here */,
			conn_state->io_completion_channel /* which IO completion channel */, 
			0 /* signaling vector, not used here*/);
	if (!conn_state->client_cq) {
		// rdma_error("Failed to create CQ, errno: %d \n", -errno);
		return -errno;
	}
	debug("CQ created at %p with %d elements \n", conn_state->client_cq, conn_state->client_cq->cqe);
	ret = ibv_req_notify_cq(conn_state->client_cq, 0);
	if (ret) {
		// rdma_error("Failed to request notifications, errno: %d\n", -errno);
		return -errno;
	}
       /* Now the last step, set up the queue pair (send, recv) queues and their capacity.
         * The capacity here is define statically but this can be probed from the 
	 * device. We just use a small number as defined in rdma_common.h */
       bzero(&conn_state->qp_init_attr, sizeof(conn_state->qp_init_attr));
       conn_state->qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
       conn_state->qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
       conn_state->qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
       conn_state->qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
       conn_state->qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
       /* We use same completion queue, but one can use different queues */
       conn_state->qp_init_attr.recv_cq = conn_state->client_cq; /* Where should I notify for receive completion operations */
       conn_state->qp_init_attr.send_cq = conn_state->client_cq; /* Where should I notify for send completion operations */
       /*Lets create a QP */
       ret = rdma_create_qp(conn_state->cm_client_id /* which connection id */,
		       conn_state->pd /* which protection domain*/,
		       &conn_state->qp_init_attr /* Initial attributes */);
	if (ret) {
		// rdma_error("Failed to create QP, errno: %d \n", -errno);
	       return -errno;
	}
	conn_state->client_qp = conn_state->cm_client_id->qp;
	debug("QP created at %p \n", client_qp);
	return 0;
}

/* Pre-posts a receive buffer before calling rdma_connect () */
static int client_pre_post_recv_buffer(struct Connection* conn_state)
{
	int ret = -1;
	conn_state->server_metadata_mr = rdma_buffer_register(conn_state->pd,
			&conn_state->server_metadata_attr,
			sizeof(conn_state->server_metadata_attr),
			(IBV_ACCESS_LOCAL_WRITE));
	if(!conn_state->server_metadata_mr){
		// rdma_error("Failed to setup the server metadata mr , -ENOMEM\n");
		return -ENOMEM;
	}
	conn_state->server_recv_sge.addr = (uint64_t) conn_state->server_metadata_mr->addr;
	conn_state->server_recv_sge.length = (uint32_t) conn_state->server_metadata_mr->length;
	conn_state->server_recv_sge.lkey = (uint32_t) conn_state->server_metadata_mr->lkey;
	/* now we link it to the request */
	bzero(&conn_state->server_recv_wr, sizeof(conn_state->server_recv_wr));
	conn_state->server_recv_wr.sg_list = &conn_state->server_recv_sge;
	conn_state->server_recv_wr.num_sge = 1;
	ret = ibv_post_recv(conn_state->client_qp /* which QP */,
		      &conn_state->server_recv_wr /* receive work request*/,
		      &conn_state->bad_server_recv_wr /* error WRs */);
	if (ret) {
		// rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
		return ret;
	}
	debug("Receive buffer pre-posting is successful \n");
	return 0;
}

/* Connects to the RDMA server */
static int client_connect_to_server(struct Connection* conn_state) 
{
	struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	bzero(&conn_param, sizeof(conn_param));
	conn_param.initiator_depth = 5;
	conn_param.responder_resources = 5;
	conn_param.retry_count = 3; // if fail, then how many times to retry
	ret = rdma_connect(conn_state->cm_client_id, &conn_param);
	if (ret) {
		// rdma_error("Failed to connect to remote host , errno: %d\n", -errno);
		return -errno;
	}
	debug("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED\n");
	ret = process_rdma_cm_event(conn_state->cm_event_channel_client, 
			RDMA_CM_EVENT_ESTABLISHED,
			&cm_event);
	if (ret) {
		// rdma_error("Failed to get cm event, ret = %d \n", ret);
	       return ret;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		// rdma_error("Failed to acknowledge cm event, errno: %d\n", -errno);
		return -errno;
	}
	return 0;
}

/* Exchange buffer metadata with the server. The client sends its, and then receives
 * from the server. The client-side metadata on the server is _not_ used because
 * this program is client driven. But it shown here how to do it for the illustration
 * purposes
 */
static int client_xchange_metadata_with_server(char* src, uint32_t src_len_bytes, struct Connection* conn_state)
{
	struct ibv_wc wc[2];
	int ret = -1;
	conn_state->client_src_mr = rdma_buffer_register(conn_state->pd,
			src,
			src_len_bytes,
			(ibv_access_flags)(IBV_ACCESS_LOCAL_WRITE|
			 IBV_ACCESS_REMOTE_READ|
			 IBV_ACCESS_REMOTE_WRITE));
	if(!conn_state->client_src_mr){
		// rdma_error("Failed to register the first buffer, ret = %d \n", ret);
		return ret;
	}
	// cout << "Buffer registered successfully" << endl;
	/* we prepare metadata for the first buffer */
	conn_state->client_metadata_attr.address = (uint64_t) conn_state->client_src_mr->addr; 
	conn_state->client_metadata_attr.length = conn_state->client_src_mr->length; 
	conn_state->client_metadata_attr.stag.local_stag = conn_state->client_src_mr->lkey;
	/* now we register the metadata memory */
	conn_state->client_metadata_mr = rdma_buffer_register(conn_state->pd,
			&conn_state->client_metadata_attr,
			sizeof(conn_state->client_metadata_attr),
			IBV_ACCESS_LOCAL_WRITE);
	if(!conn_state->client_metadata_mr) {
		// rdma_error("Failed to register the client metadata buffer, ret = %d \n", ret);
		return ret;
	}
	// cout << "Metadata buffer registered successfully" << endl;

	/* now we fill up SGE */
	conn_state->client_send_sge.addr = (uint64_t) conn_state->client_metadata_mr->addr;
	conn_state->client_send_sge.length = (uint32_t) conn_state->client_metadata_mr->length;
	conn_state->client_send_sge.lkey = conn_state->client_metadata_mr->lkey;
	/* now we link to the send work request */
	bzero(&conn_state->client_send_wr, sizeof(conn_state->client_send_wr));
	conn_state->client_send_wr.sg_list = &conn_state->client_send_sge;
	conn_state->client_send_wr.num_sge = 1;
	conn_state->client_send_wr.opcode = IBV_WR_SEND;
	conn_state->client_send_wr.send_flags = IBV_SEND_SIGNALED;
	/* Now we post it */
	ret = ibv_post_send(conn_state->client_qp, 
		       &conn_state->client_send_wr,
	       &conn_state->bad_client_send_wr);
	if (ret) {
		// rdma_error("Failed to send client metadata, errno: %d \n", -errno);
		return -errno;
	}
	// cout << "Metadata sent successfully" << endl;

	/* at this point we are expecting 2 work completion. One for our 
	 * send and one for recv that we will get from the server for 
	 * its buffer information */
	ret = process_work_completion_events(conn_state->io_completion_channel, 
			wc, 2);
	if(ret != 2) {
		// rdma_error("We failed to get 2 work completions , ret = %d \n", ret);
		return ret;
	}
	// cout << "Work completion received successfully" << endl;
	debug("Server sent us its buffer location and credentials, showing \n");
	show_rdma_buffer_attr(&conn_state->server_metadata_attr);
	return 0;
}

/* This function does :
 * 1) Prepare memory buffers for RDMA operations 
 * 1) RDMA write from src -> remote buffer 
 */ 
static int client_remote_memory_ops(struct Connection* conn_state)
{
	struct ibv_wc wc;
	int ret = -1;
	/* Step 1: is to copy the local buffer into the remote buffer. We will 
	 * reuse the previous variables. */
	/* now we fill up SGE */
	conn_state->client_send_sge.addr = (uint64_t) conn_state->client_src_mr->addr;
	conn_state->client_send_sge.length = (uint32_t) conn_state->client_src_mr->length;
	conn_state->client_send_sge.lkey = conn_state->client_src_mr->lkey;
	/* now we link to the send work request */
	bzero(&conn_state->client_send_wr, sizeof(conn_state->client_send_wr));
	conn_state->client_send_wr.sg_list = &conn_state->client_send_sge;
	conn_state->client_send_wr.num_sge = 1;
	conn_state->client_send_wr.opcode = IBV_WR_RDMA_WRITE;
	conn_state->client_send_wr.send_flags = IBV_SEND_SIGNALED;
	/* we have to tell server side info for RDMA */
	conn_state->client_send_wr.wr.rdma.rkey = conn_state->server_metadata_attr.stag.remote_stag;
	conn_state->client_send_wr.wr.rdma.remote_addr = conn_state->server_metadata_attr.address;
	/* Now we post it */
	ret = ibv_post_send(conn_state->client_qp, 
		       &conn_state->client_send_wr,
	       &conn_state->bad_client_send_wr);
	if (ret) {
		// rdma_error("Failed to write client src buffer, errno: %d \n",  -errno);
		return -errno;
	}
	// cout << "Buffer send posted successfully" << endl;

	/* at this point we are expecting 1 work completion for the write */
	ret = process_work_completion_events(conn_state->io_completion_channel, 
			&wc, 1);
	if(ret != 1) {
		// rdma_error("We failed to get 1 work completions , ret = %d \n",ret);
		return ret;
	}
	// cout << "Work completion received successfully" << endl;

	debug("Client side WRITE is complete \n");
	return 0;
}

/* This function disconnects the RDMA connection from the server and cleans up 
 * all the resources.
 */
static int client_disconnect_and_clean(struct Connection* conn_state)
{
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	/* active disconnect from the client side */
	ret = rdma_disconnect(conn_state->cm_client_id);
	if (ret) {
		// rdma_error("Failed to disconnect, errno: %d \n", -errno);
		//continuing anyways
	}
	ret = process_rdma_cm_event(conn_state->cm_event_channel_client, 
			RDMA_CM_EVENT_DISCONNECTED,
			&cm_event);
	if (ret) {
		// rdma_error("Failed to get RDMA_CM_EVENT_DISCONNECTED event, ret = %d\n", ret);
		//continuing anyways 
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		// rdma_error("Failed to acknowledge cm event, errno: %d\n", -errno);
		//continuing anyways
	}
	/* Destroy QP */
	rdma_destroy_qp(conn_state->cm_client_id);
	/* Destroy client cm id */
	ret = rdma_destroy_id(conn_state->cm_client_id);
	if (ret) {
		// rdma_error("Failed to destroy client id cleanly, %d \n", -errno);
		// we continue anyways;
	}
	/* Destroy CQ */
	ret = ibv_destroy_cq(conn_state->client_cq);
	if (ret) {
		// rdma_error("Failed to destroy completion queue cleanly, %d \n", -errno);
		// we continue anyways;
	}
	/* Destroy completion channel */
	ret = ibv_destroy_comp_channel(conn_state->io_completion_channel);
	if (ret) {
		// rdma_error("Failed to destroy completion channel cleanly, %d \n", -errno);
		// we continue anyways;
	}
	/* Destroy memory buffers */
	rdma_buffer_deregister(conn_state->server_metadata_mr);
	rdma_buffer_deregister(conn_state->client_metadata_mr);	
	rdma_buffer_deregister(conn_state->client_src_mr);	
	/* Destroy protection domain */
	ret = ibv_dealloc_pd(conn_state->pd);
	if (ret) {
		// rdma_error("Failed to destroy client protection domain cleanly, %d \n", -errno);
		// we continue anyways;
	}
	rdma_destroy_event_channel(conn_state->cm_event_channel_client);
	printf("Client resource clean up is complete \n");
	return 0;
}


#endif //RDMA_CLIENT_UTILS_H