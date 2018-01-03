/*
 ============================================================================
 Name        : LaiYangGlobalState.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello MPI World in C 
 ============================================================================
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mpi.h"
#include "types.h"

#define NORMAL 0
#define CONTROL 1
#define SNAPSHOT 2
#define FALSE 0
#define TRUE 1

int main(int argc, char* argv[]) {
	int my_rank; /* rank of process */
	int p; /* number of processes */
	int source; /* rank of sender */
	int dest; /* rank of destination */
	int tag = 0; /* tag for messages */
	MPI_Status status; /* return status for receive */
	MPI_Request request;

	int i, j;
	int go_on = 1;

	// current tag - indicates if the process has taken its snapshot or not
	int my_tag = FALSE;

	// a variable, to be included in the snapshot
	// TODO: assign random value
	int x = 0;

	// recorded sent messages (only with tag = false)
	int total_sent_messages = 0;
	NormalSentMessage sent_messages[100];
	int messages_sent_on_channel;
	int messages_ids_on_channel[100];

	// recorded received messages (only with tag = false)
	int total_received_messages = 0;
	NormalReceivedMessage received_messages[100];

	// received control messages (with tag = true)
	int received_control_messages = 0;

	/* start up MPI */
	MPI_Init(&argc, &argv);

	/* find out process rank */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* find out number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &p);


	// create MPI NormalSentMessage type
	const int sent_msg_fields_number = 2;
	int sent_msg_block_lengths[] = {1, 1};
	MPI_Datatype sent_msg_fields_types[] = { MPI_INT, MPI_INT };
	MPI_Datatype MPI_Normal_Sent_Message;

	MPI_Aint sent_msg_displacements[sent_msg_fields_number];
	sent_msg_displacements[0] = (MPI_Aint) offsetof(struct NormalSentMessage,
			destination);
	sent_msg_displacements[1] = (MPI_Aint) offsetof(struct NormalSentMessage,
			arrival_number);

	MPI_Type_create_struct(sent_msg_fields_number, sent_msg_block_lengths,
			sent_msg_displacements, sent_msg_fields_types, &MPI_Normal_Sent_Message);
	MPI_Type_commit(&MPI_Normal_Sent_Message);


	// create MPI NormalReceivedMessage type
	const int received_msg_fields_number = 3;
	int received_msg_block_lengths[] = {1, 1, 100};
	MPI_Datatype received_msg_fields_types[] = { MPI_INT, MPI_INT, MPI_CHAR };
	MPI_Datatype MPI_Normal_Received_Message;

	MPI_Aint received_msg_displacements[received_msg_fields_number];
	received_msg_displacements[0] = (MPI_Aint) offsetof(struct NormalReceivedMessage,
			source);
	received_msg_displacements[1] = (MPI_Aint) offsetof(struct NormalReceivedMessage,
			arrival_number);
	received_msg_displacements[2] = (MPI_Aint) offsetof(struct NormalReceivedMessage,
			content);

	MPI_Type_create_struct(received_msg_fields_number, received_msg_block_lengths,
			received_msg_displacements, received_msg_fields_types, &MPI_Normal_Received_Message);
	MPI_Type_commit(&MPI_Normal_Received_Message);


	// create MPI Control type
	const int control_fields_number = 2;
	int control_block_lengths[] = { 1, 10 };
	MPI_Datatype control_fields_types[] = { MPI_INT, MPI_INT };
	MPI_Datatype MPI_Control;

	MPI_Aint control_displacements[control_fields_number];
	control_displacements[0] = (MPI_Aint) offsetof(struct Control,
			total_messages_on_channel);
	control_displacements[1] = (MPI_Aint) offsetof(struct Control,
			messages_ids);

	MPI_Type_create_struct(control_fields_number, control_block_lengths,
			control_displacements, control_fields_types, &MPI_Control);
	MPI_Type_commit(&MPI_Control);


	// create MPI Snapshot type
	const int snapshot_fields_number = 6;
	int snapshot_block_lengths[] = { 1, 1, 1, 1, 100, 100 };
	MPI_Datatype snapshot_fields_types[] = { MPI_INT, MPI_INT, MPI_INT, MPI_INT,
		MPI_Normal_Sent_Message, MPI_Normal_Received_Message};
	MPI_Datatype MPI_Snapshot;

	MPI_Aint snapshot_displacements[snapshot_fields_number];
	snapshot_displacements[0] = (MPI_Aint) offsetof(struct Snapshot,
			process_rank);
	snapshot_displacements[1] = (MPI_Aint) offsetof(struct Snapshot, x);
	snapshot_displacements[2] = (MPI_Aint) offsetof(struct Snapshot,
			total_sent_messages);
	snapshot_displacements[3] = (MPI_Aint) offsetof(struct Snapshot,
			total_received_messages);
	snapshot_displacements[4] = (MPI_Aint) offsetof(struct Snapshot,
			sent_messages);
	snapshot_displacements[5] = (MPI_Aint) offsetof(struct Snapshot,
			received_messages);

	MPI_Type_create_struct(snapshot_fields_number, snapshot_block_lengths,
			snapshot_displacements, snapshot_fields_types, &MPI_Snapshot);
	MPI_Type_commit(&MPI_Snapshot);


	// create MPI Message type
	const int fields_number = 6;
	int block_lengths[] = { 1, 1, 1, 100, 1, 1 };
	MPI_Datatype fields_types[] = { MPI_INT, MPI_INT, MPI_INT, MPI_CHAR,
			MPI_Control, MPI_Snapshot };
	MPI_Datatype MPI_Message;

	MPI_Aint displacements[fields_number];
	displacements[0] = (MPI_Aint) offsetof(struct Message, type);
	displacements[1] = (MPI_Aint) offsetof(struct Message, tag);
	displacements[2] = (MPI_Aint) offsetof(struct Message, arrival_number);
	displacements[3] = (MPI_Aint) offsetof(struct Message, normal_content);
	displacements[4] = (MPI_Aint) offsetof(struct Message, control_content);
	displacements[5] = (MPI_Aint) offsetof(struct Message, snapshot_content);

	MPI_Type_create_struct(fields_number, block_lengths, displacements,
			fields_types, &MPI_Message);
	MPI_Type_commit(&MPI_Message);


	// start algorithm
	struct Message msg;

	// initially, each process sends some normal messages to the other processes (tag = false)
	msg.type = NORMAL;
	msg.tag = my_tag;
	for (i = 0; i < p; i++) {
		if (my_rank != i) {
			msg.arrival_number = 0;
			sprintf(msg.normal_content,
					"[source: %d] Message no. %d to process %d", my_rank, 0, i);
			MPI_Isend(&msg, 1, MPI_Message, i, tag, MPI_COMM_WORLD, &request);
			total_sent_messages++;

			NormalSentMessage sent_msg = {
					.destination = i,
					.arrival_number = 0
			};
			sent_messages[total_sent_messages - 1] = sent_msg;

			msg.arrival_number = 1;
			sprintf(msg.normal_content,
					"[source: %d] Message no. %d to process %d", my_rank, 1, i);
			MPI_Isend(&msg, 1, MPI_Message, i, tag, MPI_COMM_WORLD, &request);
			total_sent_messages++;

			sent_msg.destination = i;
			sent_msg.arrival_number = 1;
			sent_messages[total_sent_messages - 1] = sent_msg;
		}
	}

	// if I am the snapshot initiator process, I start it & also send another normal message (tag = true)
	if (my_rank == atoi(argv[1])){
		// record state
		Snapshot my_snapshot = {
			.process_rank = my_rank,
			.x = x,
			.total_sent_messages = total_sent_messages,
			.total_received_messages = total_received_messages
		};
		memcpy(my_snapshot.sent_messages, sent_messages, sizeof(sent_messages));
		memcpy(my_snapshot.received_messages, received_messages, sizeof(received_messages));

		// change my tag
		my_tag = TRUE;

		// send control messages to everyone & another normal message
		for (dest = 0; dest < p; dest++) {
			if (my_rank != dest) {
				// build the control message for each channel
				messages_sent_on_channel = 0;
				memset(messages_ids_on_channel, 0, sizeof(messages_ids_on_channel));

				for (j = 0; j < total_sent_messages; j++) {
					if (dest == sent_messages[j].destination) {
						messages_ids_on_channel[messages_sent_on_channel] = sent_messages[j].arrival_number;
						messages_sent_on_channel++;
					}
				}

				Control control_content = {
					.total_messages_on_channel = messages_sent_on_channel
				};
				memcpy(control_content.messages_ids, messages_ids_on_channel, sizeof(messages_ids_on_channel));

				msg.type = CONTROL;
				msg.tag = my_tag;
				msg.arrival_number = 1000;
				msg.control_content = control_content;

				// send it to the neighbor on the channel
				MPI_Isend(&msg, 1, MPI_Message, dest, tag, MPI_COMM_WORLD, &request);

				// also send a normal message (with tag = true now)
				msg.type = NORMAL;
				msg.arrival_number = 2;
				sprintf(msg.normal_content,
						"[source: %d] Message no. %d to process %d", my_rank, 2, dest);
				MPI_Isend(&msg, 1, MPI_Message, dest, tag, MPI_COMM_WORLD, &request);
			}
		}
	}

	i = 0;
	while (go_on == 1) {

		MPI_Recv(&msg, 1, MPI_Message, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD,
				&status);
		source = status.MPI_SOURCE;

		switch (msg.type) {
		case NORMAL:
			if (msg.tag == FALSE) {
				// record the message
				total_received_messages++;

				NormalReceivedMessage received_msg = {
						.source = source,
						.arrival_number = msg.arrival_number
				};
				strcpy(received_msg.content, msg.normal_content);
				received_messages[total_received_messages - 1] = received_msg;
			}
			else if (msg.tag == TRUE && my_tag == FALSE) {
				// start the snapshotting process
				// record state
				Snapshot my_snapshot = {
						.process_rank = my_rank,
						.x = x,
						.total_sent_messages = total_sent_messages,
						.total_received_messages = total_received_messages
				};
				memcpy(my_snapshot.sent_messages, sent_messages, sizeof(sent_messages));
				memcpy(my_snapshot.received_messages, received_messages, sizeof(received_messages));

				// change my tag
				my_tag = TRUE;

				// send control messages to everyone & another normal message
				for (dest = 0; dest < p; dest++) {
					if (my_rank != dest) {
						// build the control message for each channel
						messages_sent_on_channel = 0;
						memset(messages_ids_on_channel, 0, sizeof(messages_ids_on_channel));

						for (j = 0; j < total_sent_messages; j++) {
							if (dest == sent_messages[j].destination) {
								messages_ids_on_channel[messages_sent_on_channel] = sent_messages[j].arrival_number;
								messages_sent_on_channel++;
							}
						}

						Control control_content = {
							.total_messages_on_channel = messages_sent_on_channel
						};
						memcpy(control_content.messages_ids, messages_ids_on_channel, sizeof(messages_ids_on_channel));

						msg.type = CONTROL;
						msg.tag = my_tag;
						msg.arrival_number = 1000;
						msg.control_content = control_content;

						// send it to the neighbor on the channel
						MPI_Isend(&msg, 1, MPI_Message, dest, tag, MPI_COMM_WORLD, &request);

						// also send a normal message (with tag = true now)
						msg.type = NORMAL;
						msg.arrival_number = 2;
						sprintf(msg.normal_content,
								"[source: %d] Message no. %d to process %d", my_rank, 2, dest);
						MPI_Isend(&msg, 1, MPI_Message, dest, tag, MPI_COMM_WORLD, &request);
					}
				}
			}
			break;

		case CONTROL:
			if (my_tag == FALSE) {
				// should also start the snapshotting process
				// but in our example, the control message should never arrive before a normal message with tag = true (i.e. pre-snapshot)
				// so we ignore this case
				// printf("[%d] I received your control message, process %d!\n", my_rank, source);
			}

			received_control_messages++;
			if (received_control_messages == p-1) {
				printf("[%d] I have received all control messages!\n", my_rank);

				// check if snapshot is final somehow
				// if it is final, send it to the initiator proces
				// terminate!
			}
		}

		i++;
//		if (my_rank == atoi(argv[1])) {
//			if (i == (p - 1) * 2) {
//				go_on = 0;
//			}
//		}
//		else {
			if (i == (p - 1) * 4) {
				go_on = 0;
			}
//		}

	}

	for (i = 0; i < total_received_messages; i++) {
		printf("[%d] msg.source: %d\n", my_rank, received_messages[i].source);
		printf("[%d] msg.arrival_number: %d\n", my_rank,
				received_messages[i].arrival_number);
		printf("[%d] msg.content: %s\n", my_rank, received_messages[i].content);
	}

//	if (my_rank == atoi(argv[1])) {
//		// I am the initiator of the snapshot
//	}
//	else {
//		// I am a normal process
//	}

	/* shut down MPI */
	MPI_Finalize();

	return 0;

	// printing snapshot messages
//	printf("control_content.total_msgs_on_channel: %d\n", msg.control_content.total_messages_on_channel);
//	printf("control_content.messages_ids: \n");
//	for (i = 0; i < msg.control_content.total_messages_on_channel; i++) {
//		printf("id: %d\n", msg.control_content.messages_ids[i]);
//	}

	// empty these out - maybe they don't need emptying?
//		memset(msg.control_content, 0, sizeof msg.control_content);
//		msg.snapshot_state_content = 0;
//		memset(msg.snapshot_messages_content, 0, sizeof msg.snapshot_messages_content);
}