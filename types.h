#ifndef TYPES_H_
#define TYPES_H_

// used for recording messages
typedef struct NormalSentMessage {
	int destination;
	int arrival_number;
} NormalSentMessage;

typedef struct NormalReceivedMessage {
	int source;
	int arrival_number;
	char content[100];
} NormalReceivedMessage;

// Control-type message content
typedef struct Control {
	int total_messages_on_channel;
	int messages_ids[10];
} Control;

// Snapshot-type message content
typedef struct Snapshot {
	int process_rank;
	int x;
	int total_sent_messages;
	int total_received_messages;
	NormalSentMessage sent_messages[100];
	NormalReceivedMessage received_messages[100];
} Snapshot;

// Messages used to communicate between the processes
// Only one of the content types (i.e. normal, control, snapshot) should be filled in
typedef struct Message {
	int type;
	int tag;
	int arrival_number;

	char normal_content[100];
	Control control_content;
	Snapshot snapshot_content;
} Message;

#endif /* TYPES_H_ */