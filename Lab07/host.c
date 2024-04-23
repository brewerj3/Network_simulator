/*
 * host.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include "net.h"
#include "man.h"
#include "host.h"
#include "packet.h"
#include "main.h"

#define MAX_FILE_BUFFER 1000
#define MAX_MSG_LENGTH 100
#define MAX_DIR_NAME 100
#define MAX_FILE_NAME 100
#define PKT_PAYLOAD_MAX 100

/* Types of packets */

struct file_buf {
    char name[MAX_FILE_NAME];
    int name_length;
    char buffer[MAX_FILE_BUFFER + 1];
    int head;
    int tail;
    int occ;
    FILE *fd;
};


/*
 * File buffer operations
 */

/* Initialize file buffer data structure */
void file_buf_init(struct file_buf *f) {
    f->head = 0;
    f->tail = MAX_FILE_BUFFER;
    f->occ = 0;
    f->name_length = 0;
}

/* 
 * Get the file name in the file buffer and store it in name 
 * Terminate the string in name with tne null character
 */
void file_buf_get_name(struct file_buf *f, char name[]) {
    int i;

    for (i = 0; i < f->name_length; i++) {
        name[i] = f->name[i];
    }
    name[f->name_length] = '\0';
}

/*
 *  Put name[] into the file name in the file buffer
 *  length = the length of name[]
 */
void file_buf_put_name(struct file_buf *f, char name[], int length) {
    int i;

    for (i = 0; i < length; i++) {
        f->name[i] = name[i];
    }
    f->name_length = length;
}

/*
 *  Add 'length' bytes n string[] to the file buffer
 */
int file_buf_add(struct file_buf *f, char string[], int length) {
    int i = 0;

    while (i < length && f->occ < MAX_FILE_BUFFER) {
        f->tail = (f->tail + 1) % (MAX_FILE_BUFFER + 1);
        f->buffer[f->tail] = string[i];
        i++;
        f->occ++;
    }
    return (i);
}

/*
 *  Remove bytes from the file buffer and store it in string[] 
 *  The number of bytes is length.
 */
int file_buf_remove(struct file_buf *f, char string[], int length) {
    int i = 0;

    while (i < length && f->occ > 0) {
        string[i] = f->buffer[f->head];
        f->head = (f->head + 1) % (MAX_FILE_BUFFER + 1);
        i++;
        f->occ--;
    }

    return (i);
}


/*
 * Operations with the manager
 */

int get_man_command(struct man_port_at_host *port, char msg[], char *c) {

    int n;
    int i;
    int k;

    n = read(port->recv_fd, msg, MAN_MSG_LENGTH); /* Get command from manager */
    if (n > 0) {  /* Remove the first char from "msg" */
        for (i = 0; msg[i] == ' ' && i < n; i++);
        *c = msg[i];
        i++;
        for (; msg[i] == ' ' && i < n; i++);
        for (k = 0; k + i < n; k++) {
            msg[k] = msg[k + i];
        }
        msg[k] = '\0';
    }
    return n;

}

/*
 * Operations requested by the manager
 */

/* Send back state of the host to the manager as a text message */
void reply_display_host_state(struct man_port_at_host *port, char dir[], int dir_valid, int host_id) {
    int n;
    char reply_msg[MAX_MSG_LENGTH];

    if (dir_valid == 1) {
        n = sprintf(reply_msg, "%s %d", dir, host_id);
    } else {
        n = sprintf(reply_msg, "None %d", host_id);
    }

    write(port->send_fd, reply_msg, n);
}



/* Job queue operations */

/* Add a job to the job queue */
void job_q_add(struct job_queue *j_q, struct host_job *j) {
    if (j_q->head == NULL) {
        j_q->head = j;
        j_q->tail = j;
        j_q->occ = 1;
    } else {
        (j_q->tail)->next = j;
        j->next = NULL;
        j_q->tail = j;
        j_q->occ++;
    }
}

/* Remove job from the job queue, and return pointer to the job*/
struct host_job *job_q_remove(struct job_queue *j_q) {
    struct host_job *j;

    if (j_q->occ == 0) return (NULL);
    j = j_q->head;
    j_q->head = (j_q->head)->next;
    j_q->occ--;
    return (j);
}

/* Initialize job queue */
void job_q_init(struct job_queue *j_q) {
    j_q->occ = 0;
    j_q->head = NULL;
    j_q->tail = NULL;
}

int job_q_num(struct job_queue *j_q) {
    return j_q->occ;
}

/*
 *  Main 
 */

_Noreturn void host_main(int host_id) {

/* State */
    char dir[MAX_DIR_NAME];
    int dir_valid = 0;

    char man_msg[MAN_MSG_LENGTH];
    char man_reply_msg[MAN_MSG_LENGTH];
    char man_cmd;
    struct man_port_at_host *man_port;  // Port to the manager

    struct net_port *node_port_list;
    struct net_port **node_port;  // Array of pointers to node ports
    int node_port_num;            // Number of node ports

    int ping_reply_received;

    bool dns_register_received;
    bool dns_lookup_received;

    int dns_lookup_response;

    char dns_register_buffer[MAX_DNS_NAME_LENGTH];
    char dns_lookup_buffer[MAX_DNS_NAME_LENGTH];

    int i, k, n;
    int dst;


    char name[MAX_FILE_NAME];
    char string[PKT_PAYLOAD_MAX + 1];

    FILE *fp;

    struct packet *in_packet; /* Incoming packet */
    struct packet *new_packet;

    struct net_port *p;
    struct host_job *new_job;
    struct host_job *new_job2;

    struct job_queue job_q;

    struct file_buf f_buf_upload;
    struct file_buf f_buf_download;

    file_buf_init(&f_buf_upload);
    file_buf_init(&f_buf_download);

/*
 * Initialize pipes 
 * Get link port to the manager
 */

    man_port = net_get_host_port(host_id);

/*
 * Create an array node_port[ ] to store the network link ports
 * at the host.  The number of ports is node_port_num
 */
    node_port_list = net_get_port_list(host_id);

    /*  Count the number of network link ports */
    node_port_num = 0;
    for (p = node_port_list; p != NULL; p = p->next) {
        node_port_num++;
    }
    /* Create memory space for the array */
    node_port = (struct net_port **) malloc(node_port_num * sizeof(struct net_port *));

    /* Load ports into the array */
    p = node_port_list;
    for (k = 0; k < node_port_num; k++) {
        node_port[k] = p;
        p = p->next;
    }

/* Initialize the job queue */
    job_q_init(&job_q);

    while (1) {
        /* Execute command from manager, if any */

        /* Get command from manager */
        n = get_man_command(man_port, man_msg, &man_cmd);

        /* Execute command */
        if (n > 0) {
            switch (man_cmd) {
                case 's': {
                    reply_display_host_state(man_port, dir, dir_valid, host_id);
                    break;
                }

                case 'm': {
                    dir_valid = 1;
                    for (i = 0; man_msg[i] != '\0'; i++) {
                        dir[i] = man_msg[i];
                    }
                    dir[i] = man_msg[i];
                    break;
                }

                case 'p': {
                    // Sending ping request
                    // Create new ping request packet
                    sscanf(man_msg, "%d", &dst);
                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->src = (char) host_id;
                    new_packet->dst = (char) dst;
                    new_packet->type = (char) PKT_PING_REQ;
                    new_packet->length = 0;
                    new_job = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job->packet = new_packet;
                    new_job->type = JOB_SEND_PKT_ALL_PORTS;
                    job_q_add(&job_q, new_job);

                    new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                    ping_reply_received = 0;
                    new_job2->type = JOB_PING_WAIT_FOR_REPLY;
                    new_job2->ping_timer = 10;
                    job_q_add(&job_q, new_job2);

                    break;
                }
/* =========================== Download a file to a host =========================== */
			    case 'd': {
                    sscanf(man_msg, "%d %s", &dst, name);
                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->src = (char) host_id;
                    new_packet->dst = (char) dst;
                    new_packet->type = PKT_FILE_DOWNLOAD_REQ;
                    for (i = 0; name[i] != '\0'; i++) {
                        new_packet->payload[i] = name[i];
                    }
                    new_packet->payload[i] = '\0';
                    new_packet->length = i;
                    new_job = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job->packet = new_packet;
                    new_job->type = JOB_SEND_PKT_ALL_PORTS;
                    new_job->file_upload_dst = dst;    //the problem
                    for (i = 0; name[i] != '\0'; i++) {
                        new_job->fname_download[i] = name[i];
                    }
                    new_job->fname_download[i] = '\0';
                    job_q_add(&job_q, new_job);
//				    printf("send download job to host queue\nname: %s\n",new_job->fname_download);
                    break;
                }
/* =========================== Upload a file to a host =========================== */    
                case 'u': {
                    /* Upload a file to a host */
                    sscanf(man_msg, "%d %s", &dst, name);
                    new_job = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job->type = JOB_FILE_UPLOAD_SEND;
                    new_job->file_upload_dst = dst;
                    for (i = 0; name[i] != '\0'; i++) {
                        new_job->fname_upload[i] = name[i];
                    }
                    new_job->fname_upload[i] = '\0';
                    job_q_add(&job_q, new_job);

                    break;
                }
/* =========================== Register a domain name with DNS server=============*/
                case 'r': {
                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->src = (char) host_id;
                    new_packet->dst = (char) 100;
                    new_packet->type = (char) PKT_DNS_REGISTER;
                    for (i = 0; man_msg[i] != '\0' && i < PKT_PAYLOAD_MAX; i++) {
                        new_packet->payload[i] = man_msg[i];
                    }
                    new_packet->payload[i] = '\0';
                    new_packet->length = i;

                    // Create a job to send the packet
                    new_job = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job->packet = new_packet;
                    new_job->type = JOB_SEND_PKT_ALL_PORTS;
                    job_q_add(&job_q, new_job);

                    // Create a second job to wait for reply
                    new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                    dns_register_received = false;
                    memset(string, 0, PKT_PAYLOAD_MAX);
                    new_job2->type = JOB_DNS_REGISTER_WAIT_FOR_REPLY;
                    new_job2->ping_timer = 40;
                    job_q_add(&job_q, new_job2);
                    break;
                }
                case 'l': {
                    // Create a new packet
                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->src = (char) host_id;
                    new_packet->dst = DNS_SERVER_ID;  // DNS server always has the same ID
                    new_packet->type = (char) PKT_DNS_LOOKUP;
                    strncpy(new_packet->payload, man_msg, PKT_PAYLOAD_MAX);
                    new_packet->length = strnlen(new_packet->payload, PKT_PAYLOAD_MAX);

                    // Create a job to send the packet
                    new_job = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job->packet = new_packet;
                    new_job->type = JOB_SEND_PKT_ALL_PORTS;
                    job_q_add(&job_q, new_job);

                    // Create a second job to wait for reply
                    new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                    dns_lookup_received = false;
                    memset(dns_lookup_buffer, 0, MAX_DNS_NAME_LENGTH);
                    strncpy(new_job2->fname_download, man_msg, MAX_FILE_NAME);
                    new_job2->type = JOB_DNS_LOOKUP_WAIT_FOR_REPLY;
                    new_job2->ping_timer = 40;
                    job_q_add(&job_q, new_job2);
                    break;
                }
                case 'P': {
                    char domain_name[MAX_DNS_NAME_LENGTH];
                    strncpy(domain_name, man_msg, MAX_DNS_NAME_LENGTH);
                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->src = (char) host_id;
                    new_packet->dst = DNS_SERVER_ID;
                    new_packet->type = PKT_DNS_LOOKUP;
                    strncpy(new_packet->payload, domain_name, PKT_PAYLOAD_MAX);
                    new_packet->length = strnlen(new_packet->payload, PKT_PAYLOAD_MAX);

                    // Create a job to send the packet
                    new_job = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job->packet = new_packet;
                    new_job->type = JOB_SEND_PKT_ALL_PORTS;
                    job_q_add(&job_q, new_job);

                    // Create second job to wait for reply
                    new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                    dns_lookup_received = false;
                    new_job2->type = JOB_DNS_PING_WAIT_FOR_REPLY;
                    new_job2->ping_timer = 40;
                    job_q_add(&job_q, new_job2);
                    break;
                }
                case 'D': {
                    char domain_name[MAX_DNS_NAME_LENGTH];
                    char file_name[MAX_FILE_NAME];
                    sscanf(man_msg, "%s %s", domain_name, file_name);

                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->src = (char) host_id;
                    new_packet->dst = DNS_SERVER_ID;
                    new_packet->type = (char) PKT_DNS_LOOKUP;
                    strncpy(new_packet->payload, domain_name, PKT_PAYLOAD_MAX);
                    new_packet->length = strnlen(new_packet->payload, PKT_PAYLOAD_MAX);

                    // Create a job to send packet
                    new_job = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job->packet = new_packet;
                    new_job->type = JOB_SEND_PKT_ALL_PORTS;
                    job_q_add(&job_q, new_job);

                    // Create a second job to wait for reply
                    new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                    dns_lookup_received = false;
                    strncpy(new_job2->fname_download, name, PKT_PAYLOAD_MAX);
                    new_job2->type = JOB_DNS_DOWNLOAD_WAIT_FOR_REPLY;
                    new_job2->ping_timer = 40;
                    job_q_add(&job_q, new_job);
                    break;
                }
                default:;
            }
        }

        /*
         * Get packets from incoming links and translate to jobs
           * Put jobs in job queue
          */

        for (k = 0; k < node_port_num; k++) { /* Scan all ports */

            in_packet = (struct packet *) malloc(sizeof(struct packet));
            n = packet_recv(node_port[k], in_packet);

            if ((n > 0) && ((int) in_packet->dst == host_id)) {
                new_job = (struct host_job *) malloc(sizeof(struct host_job));
                new_job->in_port_index = k;
                new_job->packet = in_packet;

                switch (in_packet->type) {
                    /* Consider the packet type */

/* =========================== Ping =========================== */
                    case (char) PKT_PING_REQ: {
                        new_job->type = JOB_PING_SEND_REPLY;
                        job_q_add(&job_q, new_job);
                        break;
                    }

                    case (char) PKT_PING_REPLY: {
                        ping_reply_received = 1;
                        free(in_packet);
                        free(new_job);
                        break;
                    }
/* =========================== Upload =========================== */
				    case (char) PKT_FILE_UPLOAD_START:
				        new_job->type = JOB_FILE_UPLOAD_RECV_START;
					    job_q_add(&job_q, new_job);
					    break;
				    
                    case (char) PKT_FILE_UPLOAD_MID:
					    new_job->type = JOB_FILE_DOWNLOAD_RECV_MID;
					    job_q_add(&job_q, new_job);
					    break;

				    case (char) PKT_FILE_UPLOAD_END:
					    new_job->type = JOB_FILE_UPLOAD_RECV_END;
					    job_q_add(&job_q, new_job);
					    break;
/* =========================== Download =========================== */
				    case (char) PKT_FILE_DOWNLOAD_REQ:
					    new_job->type = JOB_FILE_DOWNLOAD_SEND;
					    job_q_add(&job_q, new_job);
					    break;

				    case (char) PKT_FILE_DOWNLOAD_START:
					    new_job->type = JOB_FILE_DOWNLOAD_RECV_START;
					    job_q_add(&job_q, new_job);
					    break;

				    case (char) PKT_FILE_DOWNLOAD_END:
					    new_job->type = JOB_FILE_DOWNLOAD_RECV_END;
					    job_q_add(&job_q, new_job);
					    break;	
/* ================================================================ */
                    case (char) PKT_DNS_REGISTER_REPLY: {
                        dns_register_received = true;
                        strncpy(dns_register_buffer, in_packet->payload, PKT_PAYLOAD_MAX);
                        free(new_job->packet);
                        free(new_job);
                        break;
                    }
                    case (char) PKT_DNS_LOOKUP_REPLY: {
                        dns_lookup_received = true;
                        strncpy(dns_lookup_buffer, new_job->packet->payload, PKT_PAYLOAD_MAX);
                        free(new_job->packet);
                        free(new_job);
                    }
                    default: {
                        free(in_packet);
                        free(new_job);
                    }
                }
            } else {
                free(in_packet);
            }
        }

        /*
          * Execute one job in the job queue
          */

        if (job_q_num(&job_q) > 0) {

            /* Get a new job from the job queue */
            new_job = job_q_remove(&job_q);


            /* Send packet on all ports */
            switch (new_job->type) {

                /* Send packets on all ports */
                case JOB_SEND_PKT_ALL_PORTS: {
                    for (k = 0; k < node_port_num; k++) {
                        packet_send(node_port[k], new_job->packet);
                    }
                    free(new_job->packet);
                    free(new_job);
                    break;
                }

                    /* The next three jobs deal with the pinging process */
                case JOB_PING_SEND_REPLY: {
                    /* Send a ping reply packet */

                    /* Create ping reply packet */
                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->dst = new_job->packet->src;
                    new_packet->src = (char) host_id;
                    new_packet->type = PKT_PING_REPLY;
                    new_packet->length = 0;

                    /* Create job for the ping reply */
                    new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                    new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                    new_job2->packet = new_packet;

                    /* Enter job in the job queue */
                    job_q_add(&job_q, new_job2);

                    /* Free old packet and job memory space */
                    free(new_job->packet);
                    free(new_job);
                    break;
                }

                case JOB_PING_WAIT_FOR_REPLY: {
                    /* Wait for a ping reply packet */

                    if (ping_reply_received == 1) {
                        n = sprintf(man_reply_msg, "Ping acked!");
                        man_reply_msg[n] = '\0';
                        write(man_port->send_fd, man_reply_msg, n + 1);
                        free(new_job);
                    } else if (new_job->ping_timer > 1) {
                        new_job->ping_timer--;
                        job_q_add(&job_q, new_job);
                    } else { /* Time out */
                        n = sprintf(man_reply_msg, "Ping time out!");
                        man_reply_msg[n] = '\0';
                        write(man_port->send_fd, man_reply_msg, n + 1);
                        free(new_job);
                    }

                    break;
                }


                    /* The next three jobs deal with uploading a file */

                    /* This job is for the sending host */
                case JOB_FILE_UPLOAD_SEND: {
                    /* Open file */
                    if (dir_valid == 1) {
                        n = sprintf(name, "./%s/%s", dir, new_job->fname_upload);
                        name[n] = '\0';
                        fp = fopen(name, "r");
                        fseek(fp, 0L, SEEK_END);
                        int s = ftell(fp);
                //		printf("file pointer: %d\n",s);
                        rewind(fp);
                        
                        if (fp != NULL) {
                            /* 
                            * Create first packet which
                            * has the file name 
                            */
                            new_packet = (struct packet *) malloc(sizeof(struct packet));
                            new_packet->dst = new_job->file_upload_dst;
                            new_packet->src = (char) host_id;
                            new_packet->type  = PKT_FILE_UPLOAD_START;
                            for (i=0; new_job->fname_upload[i]!= '\0'; i++) {
                                new_packet->payload[i] = new_job->fname_upload[i];
                            }
                            new_packet->length = i;

                            /* 
                            * Create a job to send the packet
                            * and put it in the job queue
                            */
                            new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                            new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                            new_job2->packet = new_packet;
                            job_q_add(&job_q, new_job2);
                            /* 
                            * Create the second packet which
                            * has the file contents
                            */
                            while(s>0) {
                                s -= PKT_PAYLOAD_MAX;
                                new_packet = (struct packet *) malloc(sizeof(struct packet));
                                new_packet->dst = new_job->file_upload_dst;
                                new_packet->src = (char) host_id;
                                new_packet->type = PKT_FILE_UPLOAD_END;
                                n = fread(string,sizeof(char),PKT_PAYLOAD_MAX, fp);
                                
                    //			printf("sent ' %s ' to %d\n",string, new_packet->dst);
                                
                                string[n] = '\0';
                                for (i=0; i<n; i++) {
                                    new_packet->payload[i] = string[i];
                                }
                                new_packet->length = n;
                            
                                /*
                                * Create a job to send the packet
                                * and put the job in the job queue
                                */
                                new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                                new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                                new_job2->packet = new_packet;
                                job_q_add(&job_q, new_job2);
                                //	usleep(TENMILLISEC);
                            }

                            fclose(fp);
        //					free(new_job->packet);
        //					free(new_job);
                        }
                        else {  
                            /* Didn't open file */
                        }
                        
                        
                    }
                break;
                }

                    /* The next two jobs are for the receving host */

                case JOB_FILE_UPLOAD_RECV_START: {

                    /* Initialize the file buffer data structure */
                    file_buf_init(&f_buf_upload);

                    /*
                     * Transfer the file name in the packet payload
                     * to the file buffer data structure
                     */
                    file_buf_put_name(&f_buf_upload, new_job->packet->payload, new_job->packet->length);

                    free(new_job->packet);
                    free(new_job);
                    break;
                }

                case JOB_FILE_UPLOAD_RECV_END: {
                    /*
                     * Download packet payload into file buffer
                     * data structure
                     */
                    file_buf_add(&f_buf_upload, new_job->packet->payload, new_job->packet->length);

                    free(new_job->packet);
                    free(new_job);

                    if (dir_valid == 1) {

                        /*
                         * Get file name from the file buffer
                         * Then open the file
                         */
                        file_buf_get_name(&f_buf_upload, string);
                        n = sprintf(name, "./%s/%s", dir, string);
                        name[n] = '\0';

                        fp = fopen(name, "a+");

                        if (fp != NULL) {
                            /*
                             * Write contents in the file
                             * buffer into file
                             */

                            while (f_buf_upload.occ > 0) {
                                n = file_buf_remove(&f_buf_upload, string, PKT_PAYLOAD_MAX);
                                string[n] = '\0';
                                n = fwrite(string, sizeof(char), n, fp);
                            }

                            fclose(fp);
                        }
                    }
                    break;
                }
                case JOB_FILE_DOWNLOAD_SEND: {
                    if (dir_valid == 1) {
                        n = sprintf(name, "./%s/%s", dir , new_job->packet->payload);
                        name[n] = '\0';
//printf("name array: %s\n", name);
                        fp = fopen(name, "r");
                        fseek(fp, 0L, SEEK_END);
                        int s = ftell(fp);
//printf("file pointer: %d\n",s);
                        rewind(fp);
                        if (fp != NULL) {

                            new_packet = (struct packet *)
                                    malloc(sizeof(struct packet));
                            new_packet->dst
                                    = new_job->packet->src;
                            new_packet->src = (char) host_id;
                            new_packet->type
                                    = PKT_FILE_DOWNLOAD_START;
                            for (i=0;
                                 new_job->packet->payload[i]!= '\0';
                                 i++) {
                                new_packet->payload[i] =
                                        new_job->packet->payload[i];
                            }
                            new_packet->length = i;
//printf("download file name: %s\n", new_job->packet->payload);
                            new_job2 = (struct host_job *)
                                    malloc(sizeof(struct host_job));
                            new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                            new_job2->packet = new_packet;
                            job_q_add(&job_q, new_job2);

                            while(s>0){
                                s -= PKT_PAYLOAD_MAX;
                                new_packet = (struct packet *)
                                        malloc(sizeof(struct packet));
                                new_packet->dst
                                        = new_job->packet->src;

//printf("sent file to %d\n", new_packet->dst);

                                new_packet->src = (char) host_id;
                                new_packet->type = PKT_FILE_DOWNLOAD_END;
                                n = fread(string,sizeof(char),PKT_PAYLOAD_MAX, fp);
                                string[n] = '\0';
                                for (i=0; i<n; i++) {
                                    new_packet->payload[i]
                                            = string[i];
                                }
//	printf("sent packet: %s\n", string);
                                new_packet->length = n;

                                new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                                new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                                new_job2->packet = new_packet;
                                job_q_add(&job_q, new_job2);
                            }
                            fclose(fp);
//					free(new_job->packet);
//					free(new_job);
                        }
                    }else{
                        printf("Please set host's directory\n");
                    }
                    break;
                }

                case JOB_FILE_DOWNLOAD_RECV_START: {
                    file_buf_init(&f_buf_download);
                    file_buf_put_name(&f_buf_download, new_job->packet->payload, new_job->packet->length);

                    free(new_job->packet);
                    free(new_job);
                    break;
                }
                case JOB_FILE_DOWNLOAD_RECV_END: {
                    file_buf_add(&f_buf_download, new_job->packet->payload, new_job->packet->length);


                    if (dir_valid == 1) {
                        file_buf_get_name(&f_buf_download, string);
                        n = sprintf(name, "./%s/%s", dir, string);
                        name[n] = '\0';
                        fp = fopen(name, "a+");
                        if (fp != NULL) {

                            while (f_buf_download.occ > 0) {
                                n = file_buf_remove(&f_buf_download, string, PKT_PAYLOAD_MAX);
                                string[n] = '\0';
                                n = fwrite(string, sizeof(char), n, fp);
                            }
                            fclose(fp);
                        }
                    }
                    break;
                }

                case JOB_DNS_REGISTER_WAIT_FOR_REPLY: {
                    if (dns_register_received) {
                        //printf("\nDNS register buffer: |%s\n", dns_register_buffer);
                        //printf("Length%zu\n", strnlen(dns_register_buffer, 100));
                        memset(man_reply_msg, 0, MAN_MSG_LENGTH);
                        switch (dns_register_buffer[0]) {
                            case 'S': {
                                n = sprintf(man_reply_msg, "Successfully registered domain name");
                                break;
                            }
                            case 'L': {
                                n = sprintf(man_reply_msg, "Failed to register: Name too long");
                                break;
                            }
                            case 'I': {
                                n = sprintf(man_reply_msg, "Failed to register: Name is Invalid");
                                break;
                            }
                            case 'A': {
                                n = sprintf(man_reply_msg, "Failed to register: Already registered");
                                break;
                            }
                            default: {
                                n = sprintf(man_reply_msg, "Failed to parse DNS registration response");
                                break;
                            }
                        }
                        man_reply_msg[n] = '\0';
                        write(man_port->send_fd, man_reply_msg, n + 1);
                        free(new_job);
                        memset(dns_register_buffer, 0, MAX_DNS_NAME_LENGTH);
                    } else if (new_job->ping_timer > 1) {
                        new_job->ping_timer--;
                        job_q_add(&job_q, new_job);
                    } else {
                        strncpy(man_reply_msg, "DNS registration time out", MAN_MSG_LENGTH);
                        write(man_port->send_fd, man_reply_msg, strnlen(man_reply_msg, MAN_MSG_LENGTH));
                        free(new_job);
                    }
                    break;
                }
                case JOB_DNS_LOOKUP_WAIT_FOR_REPLY: {
                    if (dns_lookup_received) {
                        if (strncmp(dns_lookup_buffer, "FAIL", 4) == 0) {
                            n = sprintf(man_reply_msg, "DNS lookup failed");
                            man_reply_msg[n] = '\0';
                        } else {
                            dns_lookup_response = (int) dns_lookup_buffer[0];
                            n = sprintf(man_reply_msg, "DNS lookup response %i.", dns_lookup_response);
                            man_reply_msg[n] = '\0';
                        }
                        write(man_port->send_fd, man_reply_msg, n + 1);
                        free(new_job);
                        memset(dns_lookup_buffer, 0, MAX_DNS_NAME_LENGTH);
                    } else if (new_job->ping_timer > 1) {
                        new_job->ping_timer--;
                        job_q_add(&job_q, new_job);
                    } else {
                        n = sprintf(man_reply_msg, "DNS lookup timeout");
                        man_reply_msg[n] = '\0';
                        write(man_port->send_fd, man_reply_msg, n + 1);
                        free(new_job);
                    }
                    break;
                }
                case JOB_DNS_PING_WAIT_FOR_REPLY: {
                    if (dns_lookup_received) {
                        if (strncmp(dns_lookup_buffer, "FAIL", 4) == 0) {
                            n = sprintf(man_reply_msg, "DNS lookup failed");
                            man_reply_msg[n] = '\0';
                            write(man_port->send_fd, man_reply_msg, n + 1);
                            free(new_job);
                        } else {
                            // get host id using the returned value
                            dns_lookup_response = (int) dns_lookup_buffer[0];

                            // Create a new packet to request ping
                            new_packet = (struct packet *) malloc(sizeof(struct packet));
                            new_packet->src = (char) host_id;
                            new_packet->dst = (char) dns_lookup_response;
                            new_packet->type = (char) PKT_PING_REQ;
                            new_packet->length = 0;

                            free(new_job);

                            // Create job to send ping req
                            new_job = (struct host_job *) malloc(sizeof(struct host_job));
                            new_job->packet = new_packet;
                            new_job->type = JOB_SEND_PKT_ALL_PORTS;
                            job_q_add(&job_q, new_job);

                            // Create job to wait for ping
                            new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                            ping_reply_received = 0;
                            new_job2->type = JOB_PING_WAIT_FOR_REPLY;
                            new_job2->ping_timer = 10;
                            job_q_add(&job_q, new_job2);
                        }
                        memset(dns_lookup_buffer, 0, MAX_DNS_NAME_LENGTH);
                        dns_lookup_received = false;
                    } else if (new_job->ping_timer > 1) {
                        new_job->ping_timer--;
                        job_q_add(&job_q, new_job);
                    } else {
                        n = sprintf(man_reply_msg, "DNS lookup time out");
                        man_reply_msg[n] = '\0';
                        write(man_port->send_fd, man_reply_msg, strnlen(man_reply_msg, MAN_MSG_LENGTH));
                        free(new_job);
                    }
                    break;
                }
                case JOB_DNS_DOWNLOAD_WAIT_FOR_REPLY: {
                    if (dns_lookup_received) {
                        if (strncmp(dns_lookup_buffer, "FAIL", 4) == 0) {
                            n = sprintf(man_reply_msg, "DNS lookup failed");
                            man_reply_msg[n] = '\0';
                            write(man_port->send_fd, man_reply_msg, n + 1);
                            free(new_job);
                        } else {
                            dns_lookup_response = (int) dns_lookup_buffer[0];
                            new_packet = (struct packet *) malloc(sizeof(struct packet));
                            new_packet->src = (char) host_id;
                            new_packet->dst = (char) dns_lookup_response;
                            new_packet->type = (char) PKT_FILE_DOWNLOAD_REQ;
                            for (i = 0; i < PAYLOAD_MAX && new_job->fname_download[i] != '\0'; i++) {
                                new_packet->payload[i] = new_job->fname_download[i];
                            }
                            new_packet->payload[i] = '\0';
                            new_packet->length = i;

                            free(new_job);

                            new_job2 = (struct host_job *) malloc(sizeof(struct host_job));
                            new_job2->packet = new_packet;
                            new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                            job_q_add(&job_q, new_job2);
                        }
                        memset(dns_lookup_buffer, 0, MAX_DNS_NAME_LENGTH);
                        dns_lookup_received = false;
                    } else if (new_job->ping_timer > 1) {
                        new_job->ping_timer--;
                        job_q_add(&job_q, new_job);
                    } else {
                        strncpy(man_reply_msg, "DNS lookup time out", MAN_MSG_LENGTH);
                        write(man_port->send_fd, man_reply_msg, strnlen(man_reply_msg, MAN_MSG_LENGTH));
                        free(new_job);
                    }
                    break;
                }
            }

        }

        /* The host goes to sleep for 10 ms */
        usleep(TENMILLISEC);

    } /* End of while loop */

}




