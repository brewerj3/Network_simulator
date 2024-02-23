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

_Noreturn void switch_main(int host_id) {
    // State
    struct net_port *node_port_list;
    struct net_port **node_port;
    int node_port_num;

    int i, k, n;

    int dst;                    // Destination of a packet

    struct packet *in_packet;   // The incoming packet
    struct packet *new_packet1;
    struct packet *new_packet2;

    struct net_port *p;
    struct switch_job *new_job;
    struct switch_job *new_job2;

    struct switch_job_queue job_q;

    node_port_list = net_get_port_list(host_id);

    // Count the number of network link ports
    node_port_num = 0;
    for (p = node_port_list; p != NULL; p = p->next) {
        node_port_num++;
    }

    // Create memory space for the array
    node_port = (struct net_port **) malloc(node_port_num * sizeof(struct net_port *));

    // Load ports into the array
    p = node_port_list;
    for (k = 0; k < node_port_num; k++) {
        node_port[k] = p;
        p = p->next;
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

        // Scan all ports
        for (k = 0; k < node_port_num; k++) {
            in_packet = (struct packet *) malloc(sizeof(struct packet));
            n = packet_recv(node_port[k], in_packet);

            if (n > 0) {    // If n > 0 there is a packet to process
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
                        packet_send(node_port[k], new_job->packet);
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
