
#define BCAST_ADDR 100
#define PAYLOAD_MAX 100
#define STRING_MAX 100
#define MAX_NAME_LENGTH 100
#define MAX_DNS_NAME_LENGTH 100
#define TENMILLISEC 10000   /* 10 millisecond sleep */

#define CONTROL_COUNT_MAX 50

#pragma once


enum NetNodeType { /* Types of network nodes */
	HOST,
	SWITCH,
    SERVER
};

enum NetLinkType { /* Types of linkls */
	PIPE,
	SOCKET
};

struct net_node { /* Network node, e.g., host or switch */
	enum NetNodeType type;
	int id;
	struct net_node *next;
};

struct net_port { /* port to communicate with another node */
	enum NetLinkType type;
	int pipe_host_id;
	int pipe_send_fd;
	int pipe_recv_fd;
	struct net_port *next;
};

/* Packet sent between nodes  */

struct packet { /* struct for a packet */
	char src;
	char dst;
	char type;
	int length;
	char payload[PAYLOAD_MAX];
};

/* Types of packets */

#define PKT_PING_REQ		    1
#define PKT_PING_REPLY		    2

#define PKT_FILE_UPLOAD_START	3
#define PKT_FILE_UPLOAD_MIDDLE  4
#define PKT_FILE_UPLOAD_END	    5

#define PKT_FILE_DOWNLOAD_REQ	6

#define PKT_DNS_REGISTER        7
#define PKT_DNS_REGISTER_REPLY  8

#define PKT_DNS_LOOKUP          9
#define PKT_DNS_LOOKUP_REPLY    10

#define PKT_CONTROL_PKT         11

// packet payload indexes
//#define PKT_ROOT_ID             0
//#define PKT_ROOT_DIST           4
//#define PKT_SENDER_TYPE         8
//#define PKT_SENDER_CHILD        9

// DNS server id
#define DNS_SERVER_ID           100

#define PKT_ROOT_ID         0
#define PKT_ROOT_DIST       sizeof(int)
#define PKT_SENDER_TYPE     (sizeof(int) * 2)
#define PKT_SENDER_CHILD    ((sizeof(int) * 2) + sizeof(char))
#define PKT_CONTROL_LENGTH  ((sizeof(int) * 2) + sizeof(char) + 1)
