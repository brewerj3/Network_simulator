/* 
 * host.h 
 */

#pragma once

enum host_job_type {
	JOB_SEND_PKT_ALL_PORTS = 1,
	JOB_PING_SEND_REPLY,
	JOB_PING_WAIT_FOR_REPLY,
	JOB_FILE_UPLOAD_SEND,
	JOB_FILE_UPLOAD_RECV_START,
    JOB_FILE_UPLOAD_RECV_MIDDLE,
	JOB_FILE_UPLOAD_RECV_END,
    JOB_DNS_REGISTER_WAIT_FOR_REPLY,
    JOB_DNS_LOOKUP_WAIT_FOR_REPLY,
    JOB_DNS_PING_WAIT_FOR_REPLY,
    JOB_DNS_DOWNLOAD_WAIT_FOR_REPLY
};

struct host_job {
	enum host_job_type type;
	struct packet *packet;
	int in_port_index;
	int out_port_index;
	char fname_download[100];
	char fname_upload[100];
	int ping_timer;
	int file_upload_dst;
	struct host_job *next;
};


struct job_queue {
	struct host_job *head;
	struct host_job *tail;
	int occ;
};

_Noreturn void host_main(int host_id);


