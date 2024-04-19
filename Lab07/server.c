///////////////////////////////////////////////////////////////////////////////
///         University of Hawaii, College of Engineering
/// @brief  Network_simulator_02 - 2024
///
/// @file server.c
/// @version 1.0
///
/// @author Joshua Brewer <brewerj3@hawaii.edu> <joshuabrewer784@gmail.com>
/// @date   18_Apr_2024
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "server.h"
#include "packet.h"
#include "net.h"

#define MAX_NAME_LENGTH 100
#define NAME_TABLE_SIZE 256

typedef enum {
    SUCCESS,
    NAME_TOO_LONG,
    INVALID_NAME,
    ALREADY_REGISTERED
} RegisterAttempt;

typedef enum {
    JOB_SEND_PKT_ALL_PORTS,
    JOB_PING_SEND_REPLY,
    JOB_REGISTER_NEW_DOMAIN,
    JOB_DNS_PING_REQ,
} ServerJobType;

struct server_job {
    ServerJobType type;
    struct packet *packet;
    int in_port_index;
    int out_port_index;
    struct server_job *next;
};

typedef struct {
    struct server_job *head;
    struct server_job *tail;
    int occ;
} ServerJobQueue;

void server_add_job_queue(ServerJobQueue *job_q, struct server_job *job) {
    if (job_q->head == NULL) {
        job_q->head = job;
        job_q->tail = job;
        job_q->occ = 1;
    } else {
        job_q->tail->next = job;
        job->next = NULL;
        job_q->tail = job;
        job_q->occ++;
    }
}

struct server_job *server_job_queue_remove(ServerJobQueue *job_q) {
    struct server_job *job;
    if (job_q->head == NULL) return NULL;
    job = job_q->head;
    job_q->head = (job_q->head)->next;
    job_q->occ--;
    return job;
}

void server_job_q_init(ServerJobQueue *job_q) {
    job_q->occ = 0;
    job_q->head = NULL;
    job_q->tail =NULL;
}

int server_job_q_num(ServerJobQueue *job_q) {
    return job_q->occ;
}

void server_main(int server_id) {
    struct net_port *node_port_list;
    struct net_port **node_port;

    int node_port_num;

    int i, j, k, n;

    struct packet *in_packet;
    struct packet *new_packet;

    struct net_port *p;
    struct server_job *new_job;
    struct server_job *new_job2;

    RegisterAttempt registration_attempt_status;

    ServerJobQueue job_q;

    // Create DNS naming table
    bool is_registered[NAME_TABLE_SIZE];
    char name_table[NAME_TABLE_SIZE][MAX_NAME_LENGTH + 1];

    // Initialize values
    for (size_t iter = 0; iter < NAME_TABLE_SIZE; iter++) {
        memset(name_table[iter], 0, MAX_NAME_LENGTH);
        is_registered[iter] = false;
    }

    // Create an array node_port to store the network link ports at the host.
    node_port_list = net_get_port_list(server_id);

    // Count the number of network link ports
    node_port_num = 0;
    for (p = node_port_list; p != NULL; p = p->next) {
        node_port_num++;
    }

    // Create memory space for the array
    node_port = (struct net_port **) malloc(node_port_num * sizeof(struct net_port *));

    /* Load ports into the array */
    p = node_port_list;

    for (k = 0; k < node_port_num; k++) {
        node_port[k] = p;
        p = p->next;
    }

    // Initialize job queue
    server_job_q_init(&job_q);

    while (true) {

        // Get Packets and handle them
        for (k = 0; k < node_port_num; k++) {
            in_packet = (struct packet *) malloc(sizeof(struct packet));
            n = packet_recv(node_port[k], in_packet);
            if (n > 0) {
                // Handle packets
                new_job = (struct server_job *) malloc(sizeof(struct server_job));
                new_job->in_port_index = k;
                new_job->packet = in_packet;

                switch (in_packet->type) {
                    case (char) PKT_PING_REQ: {
                        new_job->type = JOB_PING_SEND_REPLY;
                        server_add_job_queue(&job_q, new_job);
                        break;
                    }
                    case (char) PKT_DNS_REGISTER: {
                        new_job->type = JOB_REGISTER_NEW_DOMAIN;
                        server_add_job_queue(&job_q, new_job);
                        break;
                    }
                    case (char) PKT_DNS_LOOKUP: {
                        new_job->type = JOB_DNS_PING_REQ;
                        server_add_job_queue(&job_q, new_job);
                        break;
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

        // Execute job in job queue
        if (server_job_q_num(&job_q) > 0) {
            // Get a job from the queue
            new_job = server_job_queue_remove(&job_q);

            // Process the job
            switch (new_job->type) {
                case JOB_SEND_PKT_ALL_PORTS: {
                    for (k = 0; k < node_port_num; k++) {
                        packet_send(node_port[k], new_job->packet);
                    }
                    free(new_job->packet);
                    free(new_job);
                    break;
                }
                case JOB_PING_SEND_REPLY: {
                    new_packet = (struct packet *) malloc(sizeof(struct packet));
                    new_packet->dst = new_job->packet->src;
                    new_packet->src = (char) server_id;
                    new_packet->type = PKT_PING_REPLY;
                    new_packet->length = 0;

                    // Create job to send the reply
                    new_job2 = (struct server_job *) malloc(sizeof(struct server_job));
                    new_job2->type = JOB_SEND_PKT_ALL_PORTS;
                    new_job2->packet = new_packet;

                    // Add new job to queue
                    server_add_job_queue(&job_q, new_job2);

                    // free old job from memory
                    free(new_job->packet);
                    free(new_job);
                    break;
                }
                case JOB_REGISTER_NEW_DOMAIN: {
                    if (strnlen(new_job->packet->payload, PAYLOAD_MAX) > MAX_NAME_LENGTH) {
                        registration_attempt_status = NAME_TOO_LONG;
                    } else if (is_registered[(int) new_job->packet->src]) {
                        registration_attempt_status = ALREADY_REGISTERED;
                    } else {
                        registration_attempt_status = SUCCESS;
                        for (i = 0; i < PAYLOAD_MAX && i < new_job->packet->length; i++) {
                            if (!isprint(new_job->packet->payload[i])) {
                                registration_attempt_status = INVALID_NAME;
                                break;
                            }
                        }
                    }
                    if (registration_attempt_status == SUCCESS) {
                        strncpy()
                    }
                    break;
                }
                case JOB_DNS_PING_REQ: {
                    break;
                }
                default: {

                }
            }
        }
    }
}
