// Developer: Alex St.Aubin
// Program: Peterson Leader Election Algorithm
// Description: 

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>

// Structures:
// Ring of uid's
struct ringNode{
	int uid, tmpUid, phase, pos;
	bool status;
	struct ringNode *link;
}*ring = NULL;

// Channel
struct chnlNode{
	int head, tail;
	int msg[100];
	sem_t mutex, sync;
}*channel;

// Global Variables:
int total;

// Function: getAgruments(int, char*)
// Description: Reads command line arguments and fill loop.
// Parameters: Argument count, argument vector.
// Returns: Pass - true
//          Fail - false
bool getArguments(int argc, char* argv[]){
	// Print usage message.
	if(argc == 1){
		printf("Usage: ./leader <inputFile.txt>\n");
		return false;
	}
	// Error if not exactly 2 arguments.
	if(argc != 2){
		printf("Error: invalid command line arguments.\n");
		return false;
	}
	// Open file and read to array.
	FILE *in;
	in = fopen(argv[1], "r");
	if(in == NULL){
		perror("Error opening file.\n");
		exit(EXIT_FAILURE);
	}
	
	total = 0;
	char ch;
	// Get total from file.
	ch = fgetc(in);
	while(ch != ' ' && ch != '\n'){
		total *= 10;
		total += ch%48;
		ch = fgetc(in);
	}
	// Initialize channel array.
	channel = malloc(total * sizeof(struct chnlNode));
	// Get ring data from file.
	int current = 0;
	int runningTotal = 0;
	while(true){
		// Read in character.
		ch = fgetc(in);
		// If whitespace.
		if(ch == ' ' || ch == '\n' || ch == EOF){
			// Save in array.
			struct ringNode  *tmp, *lastNode;
			tmp = (struct ringNode*)malloc(sizeof(struct ringNode));
			tmp->uid = runningTotal;
			tmp->tmpUid = runningTotal;
			tmp->pos = current;
			tmp->phase = 1;
			tmp->link = NULL;
			tmp->status = true;
			if(ring == NULL){
				ring = tmp;
				lastNode = tmp;
			}
			else{
				lastNode->link = tmp;
				lastNode = tmp;
			}
			// Initialize semaphores.
			sem_init(&channel[current].mutex, 1, 1);
			sem_init(&channel[current].sync, 1, 0);
			runningTotal = 0;
			current++;
			if(ch == EOF)
				break;
		}
		// If non-whitespace.
		else{
			// Place character in next available space.
			runningTotal *= 10;
			runningTotal += ch%48;
		}
	}
	return true;
}

// Function: readChannel(int)
// Description: Reads the channel at the passed iteration.
// Parameters: Iteration to read from.
// Returns: Value held at iteration.
int readChannel(int chnlNum){
	// Wait for synchronization and mutual exclusion.
	sem_wait(&channel[chnlNum].sync);
	sem_wait(&channel[chnlNum].mutex);
	// Read the value.
	int value = channel[chnlNum].msg[channel[chnlNum].head];
	channel[chnlNum].head++;
	// Post mutex and return read value.
	sem_post(&channel[chnlNum].mutex);
	return value;
}

// Function: writeChannel(int, int)
// Description: Writes the passed value to the channel at the given iteration.
// Parameters: Position in channel array, value to write.
// Returns: N/A
void writeChannel(int chnlNum, int value){
	// If last node, channel[0] is link.
	if(chnlNum == total)
		chnlNum = 0;
	// Wait for mutex.
	sem_wait(&channel[chnlNum].mutex);
	// Write the value to the channel.
	channel[chnlNum].msg[channel[chnlNum].tail] = value;
	channel[chnlNum].tail++;
	// Signal mutual exclusion and synchronize.
	sem_post(&channel[chnlNum].mutex);
	sem_post(&channel[chnlNum].sync);
}

// Function: send(void*)
// Description: Runs the Peterson Leader Election Algorithm.
// Parameters: Node to work on.
// Returns: Pointer to node.
void* send(void* bird){
	struct ringNode* myNode = (struct ringNode*)bird;
	int oneHop, twoHop, tmpId;
	bool flag;
	flag = true;
	while(flag){
		if(myNode->status == true){
			// Print phase data.
			printf("[%d][%d][%d]\n", myNode->phase, myNode->uid, myNode->tmpUid);
			// Write temp uid.
			writeChannel((myNode->pos + 1), myNode->tmpUid);
			// Read one hop temp uid.
			oneHop = readChannel(myNode->pos);
			// Write one hop temp uid.
			writeChannel((myNode->pos + 1), oneHop);
			// Read two hop temp uid.
			twoHop = readChannel(myNode->pos);
			// If leader found.
			if(oneHop == myNode->tmpUid){
				printf("leader: %d\n", myNode->uid);
				writeChannel((myNode->pos + 1), -1);
				flag = false;
				break;
			}
			// If still in the races for leader.
			else if((oneHop > twoHop) && (oneHop > myNode->tmpUid))
				myNode->tmpUid = oneHop;
			// Definitely not the leader.
			else
				myNode->status = false;
		}
		// Relay Node.
		else if(myNode->status == false){
			// Read temp uid.
			myNode->tmpUid = readChannel(myNode->pos);
			// Write temp uid.
			writeChannel((myNode->pos + 1), myNode->tmpUid);
			if(myNode->tmpUid == -1){
				flag = false;
				break;
			}
			// Read temp uid.
			myNode->tmpUid = readChannel(myNode->pos);
			// Write temp uid.
			writeChannel((myNode->pos + 1), myNode->tmpUid);
		}
		else{}
		// On to next phase.
		myNode->phase++;
	}
	return bird;
}

// Function: main(int, char*)
// Description: Tests the Peterson Leader Election Algorithm.
// Parameters: Command line arguments.
// Returns: Terminated program.
int main(int argc, char* argv[]){
	// Get command line arguments and exit if invalid.
	bool stat = getArguments(argc, argv);
	if(stat == false){
		printf("Error: getArguments() failure.\n");
		exit(1);
	}
	// Create thread pool.
	pthread_t *threadPool;
	threadPool = malloc(total * sizeof(pthread_t));
	// Current node to send to thread.
	struct ringNode *curr;
	curr = ring;
	int i;
	// Create threads.
	for(i = 0; i < total; i++){
		channel[i].head = 0;
		channel[i].tail = 0;
		pthread_create(&threadPool[i], NULL, (void*)send, (void*)curr);
		curr = curr->link;
	}
	// Wait for each thread to come back and destory its semaphores.
	for(i = 0; i < total; i++){
		pthread_join(threadPool[i], NULL);
		sem_destroy(&channel[i].mutex);
		sem_destroy(&channel[i].sync);
	}
	exit(1);
}