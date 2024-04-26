///////////////////////////////////////////////////////////////////////////////
///         University of Hawaii, College of Engineering
/// @brief  Network_simulator_02 - Spr 2024
///
/// @file switch.c
/// @version 1.0
///
/// @author Joshua Brewer <brewerj3@hawaii.edu>
/// @date   22_Feb_2024
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "switch.h"
#include "net.h"
#include "packet.h"

enum switch_job_type {
    JOB_SEND_PKT_ALL_SWITCH_PORTS, JOB_FORWARD_PACKET
};

struct switch_job {
    enum switch_job_type type;
    struct packet *packet;
    int in_port_index;
    int out_port_index;
    struct switch_job *next;
};

struct switch_job_queue {
    struct switch_job *head;
    struct switch_job *tail;
    int occ;
};

struct LookupTable {
    bool isValid[MAX_LOOKUP_TABLE_SIZE];
    int port_number[MAX_LOOKUP_TABLE_SIZE];
};

// Add a job to the switch job queue
void switch_job_q_add(struct switch_job_queue *j_q, struct switch_job *j) {
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

// Remove a job from the job queue and return pointer to the job
struct switch_job *switch_job_q_remove(struct switch_job_queue *j_q) {
    struct switch_job *j;
    if (j_q->occ == 0) return NULL;
    j = j_q->head;
    j_q->head = (j_q->head)->next;
    j_q->occ--;
    return (j);
}

// Initialize job queue
void switch_job_q_init(struct switch_job_queue *j_q) {
    j_q->occ = 0;
    j_q->head = NULL;
    j_q->tail = NULL;
}

int switch_job_q_num(struct switch_job_queue *j_q) {
    return j_q->occ;
}

_Noreturn void switch_main(int switch_id) {
    // State
    struct net_port *node_port_list;
    struct net_port **node_port;
    int node_port_num;

    int i, k, n;

    size_t control_count = 0;
    int local_root_id = switch_id;
    int local_root_dist = 0;
    int local_parent = -1;

    int dst;                    // Destination of a packet

    struct packet *in_packet;   // The incoming packet
    struct packet *new_packet1;
    struct packet *new_packet2;

    struct net_port *p;
    struct switch_job *new_job;
    struct switch_job *new_job2;

    struct switch_job_queue job_q;

    node_port_list = net_get_port_list(switch_id);

    // Count the number of network link ports
    node_port_num = 0;
    for (p = node_port_list; p != NULL; p = p->next) {
        node_port_num++;
    }

    // Create memory space for the array
    node_port = (struct net_port **) malloc(node_port_num * sizeof(struct net_port *));

    bool local_port_tree[node_port_num];

    // Load ports into the array
    p = node_port_list;
    for (k = 0; k < node_port_num; k++) {
        node_port[k] = p;
        p = p->next;
        local_port_tree[k] = false;
    }

    // Initialize the lookup table
    struct LookupTable table;
    for (i = 0; i < MAX_LOOKUP_TABLE_SIZE; i++) {
        table.isValid[i] = false;
    }

    // Initialize the job queue
    switch_job_q_init(&job_q);

    while (true) {
        // No need to get commands from the manager
        control_count++;
        if (control_count >= CONTROL_COUNT_MAX) {
            control_count = 0;

            // create a control packet to send
            new_packet1 = (struct packet *) malloc(sizeof(struct packet));
            new_packet1->src = (char) switch_id;
            new_packet1->type = (char) PKT_CONTROL_PKT;
            new_packet1->length = 0;
            int *root_id = (int *) &new_packet1->payload[PKT_ROOT_ID];
            *root_id = local_root_id;
            int *distance = (int *) &new_packet1->payload[PKT_ROOT_DIST];
            *distance = local_root_dist;
            new_packet1->payload[PKT_SENDER_TYPE] = 'S';
            new_packet1->dst = (char) 0;

            // Create a job to send the packet
            new_job = (struct switch_job *) malloc(sizeof(struct switch_job));
            new_job->packet = new_packet1;
            new_job->type = JOB_SEND_PKT_ALL_SWITCH_PORTS;
            switch_job_q_add(&job_q, new_job);
        }

        // Scan all ports
        for (k = 0; k < node_port_num; k++) {
            in_packet = (struct packet *) malloc(sizeof(struct packet));
            n = packet_recv(node_port[k], in_packet);

            if (n > 0) {    // If n > 0 there is a packet to process

                if (in_packet->type == (char) PKT_CONTROL_PKT) {
                    if (*&in_packet->payload[PKT_SENDER_TYPE] == 'S') {
                        int in_packet_root_id = (int) *&in_packet->payload[PKT_ROOT_ID];
                        int in_packet_root_dst = (int) *&in_packet->payload[PKT_ROOT_DIST];
#ifdef DEBUG
                        if (switch_id == 4) {
                            printf("in_packet_root_id = %i\n", in_packet_root_id);
                            printf("in_packet_root_dst = %i\n", in_packet_root_dst);
                            printf("src = %i\n", in_packet->src);
                        }
#endif
                        if (in_packet_root_id > local_root_id) {
                            local_root_id = in_packet_root_id;
                            local_parent = k;
                            local_root_dist = in_packet_root_dst + 1;
                        } else if (in_packet_root_id == local_root_id) {
                            if (local_root_dist > in_packet_root_dst + 1) {
                                local_parent = k;
                                local_root_dist = in_packet_root_dst + 1;
                            }
                        }
                    }
                    if (in_packet->payload[PKT_SENDER_TYPE] == 'H') {
                        local_port_tree[k] = true;
                    } else if (in_packet->payload[PKT_SENDER_TYPE] == 'S') {
                        if (local_parent == k) {
                            local_port_tree[k] = true;
                        } else if (in_packet->payload[PKT_SENDER_CHILD] == 'Y') {
                            local_port_tree[k] = true;
                        } else {
                            local_port_tree[k] = false;
                        }
                    } else {
                        local_port_tree[k] = false;
                    }
                    int source = (int) in_packet->src;
                    if (!table.isValid[source]) {
                        table.isValid[source] = true;
                        table.port_number[source] = k;
                    }
                    free(in_packet);
                } else {
                    new_job = (struct switch_job *) malloc(sizeof(struct switch_job));
                    new_job->in_port_index = k;
                    new_job->packet = in_packet;
                    dst = (int) in_packet->dst;
                    int source = (int) in_packet->src;
                    if (table.isValid[dst]) {
                        new_job->type = JOB_FORWARD_PACKET;
                        new_job->out_port_index = table.port_number[dst];
                    } else {
                        new_job->type = JOB_SEND_PKT_ALL_SWITCH_PORTS;
                    }

                    // Check if port can be added to the lookup table

                    if (!table.isValid[source]) {
                        table.isValid[source] = true;
                        table.port_number[source] = new_job->in_port_index;
                    }

                    // Add the job to the queue
                    switch_job_q_add(&job_q, new_job);
                }
            } else {
                free(in_packet);
            }

        }

        // Execute one job in the queue
        if (switch_job_q_num(&job_q) > 0) {

            // Get a job from the queue
            new_job = switch_job_q_remove(&job_q);

            // Send packets
            switch (new_job->type) {
                case JOB_SEND_PKT_ALL_SWITCH_PORTS: {
                    for (k = 0; k < node_port_num; k++) {
                        if (new_job->packet->type == (char) PKT_CONTROL_PKT) {
                            if (local_parent == k) {
                                new_job->packet->payload[PKT_SENDER_CHILD] = 'Y';
                            } else {
                                new_job->packet->payload[PKT_SENDER_CHILD] = 'N';
                            }
                            packet_send(node_port[k], new_job->packet);
                        } else {
                            if (local_port_tree[k] == true || local_parent == -1) {
                                if (k != new_job->in_port_index) {
                                    packet_send(node_port[k], new_job->packet);
                                }
                            }
                        }
                    }
                    free(new_job->packet);
                    free(new_job);
                    break;
                }
                case JOB_FORWARD_PACKET: {
                    packet_send(node_port[new_job->out_port_index], new_job->packet);
                    free(new_job->packet);
                    free(new_job);
                    break;
                }
                default: {
                    free(new_job->packet);
                    free(new_job);
                }
            }
        }

        // Go to sleep for 10 ms
        usleep(TENMILLISEC);
    }
}
