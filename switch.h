///////////////////////////////////////////////////////////////////////////////
///         University of Hawaii, College of Engineering
/// @brief  Network_simulator - Spr 2024
///
/// @file switch.h
/// @version 1.0
///
/// @author Joshua Brewer <brewerj3@hawaii.edu>
/// @date   22_Feb_2024
///////////////////////////////////////////////////////////////////////////////
#ifndef NETWORK_SIMULATOR_02_SWITCH_H
#define NETWORK_SIMULATOR_02_SWITCH_H

#include <stdbool.h>

#define MAX_LOOKUP_TABLE_SIZE 256

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

_Noreturn void switch_main(int host_id);

#endif //NETWORK_SIMULATOR_02_SWITCH_H
