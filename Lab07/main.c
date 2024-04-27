#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>
#include <fcntl.h>

#include "main.h"
#include "net.h"
#include "man.h"
#include "host.h"
#include "switch.h"
#include "server.h"

const char* program_name;

int main(int argc, char *argv[]) {
    program_name = argv[0];
    if (argc > 1) {
        fprintf(stderr, "%s Invalid usage: Too many arguments\n", program_name);
        exit(EXIT_FAILURE);
    } else if (argc < 1) {
        fprintf(stderr, "%s: Unknown error\n", program_name);
        exit(EXIT_FAILURE);
    }

    pid_t pid;  /* Process id */
    int k = 0;
    int status;
    struct net_node *node_list;
    struct net_node *p_node;

/*
 * Read network configuration file, which specifies
 *   - nodes, creates a list of nodes
 *   - links, creates/implements the links, e.g., using pipes or sockets
 */
    net_init();
    node_list = net_get_node_list(); /* Returns the list of nodes */


/* Create nodes, which are child processes */

    for (p_node = node_list; p_node != NULL; p_node = p_node->next) {

        pid = fork();

        if (pid == -1) {
            printf("Error:  the fork() failed\n");
            return 1;
        } else if (pid == 0) { /* The child process, which is a node  */
            if (p_node->type == HOST) {  /* Execute host routine */
                host_main(p_node->id);
            } else if (p_node->type == SWITCH) {
                /* Execute switch routine, which you have to write */
                switch_main(p_node->id);
            } else if (p_node->type == SERVER) {
                server_main(p_node->id);
            }
            return 1;
        }
    }

/* 
 * Parent process: Execute manager routine. 
 */
    man_main();


/* 
 * We reach here if the user quits the manager.
 * The following will terminate all the children processes.
 */
    kill(0, SIGKILL); /* Kill all processes */
}




