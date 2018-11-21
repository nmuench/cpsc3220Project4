#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define NOT_DIRTY 0
#define DIRTY 1
#define NOT_FOUND -1
#define SEQUENTIAL 1
#define RANDOM 0
#define READ_ACCESS 1
#define WRITE_ACCESS 0

const int diskBlockSize = 1024; //The size of a disk block in bytes
const int blocksOnDisk = 4096; //The number of blocks on the disk
const int timeForIO = 1; //The time in ms for a diskread or diskwrite
const int cacheSize = 64; //The size of the cache in blocks
const int NUM_THREADS = 40;
const int NUM_BLOCKS_SEQUENTIAL = 40;
const int NUM_BLOCKS_RANDOM = 40;
int numDiskReads = 0;
int numDiskWrites = 0;
int numBlockReads = 0;
int numBlockWrites = 0;
int numCacheHits = 0;
int numMisses = 0;
unsigned int currUse = 0;
pthread_mutex_t diskLock;
pthread_mutex_t diskQueueLock;
pthread_mutex_t queueLock;
pthread_mutex_t useLock;


typedef struct node_def
{
  int loc;
  int blockID;
  pthread_mutex_t blockLock;
  struct node_def * next;
} Node;

typedef struct disk_node_def
{
  int blockID;
  struct disk_node_def * next;
  pthread_cond_t nodeCond;
} diskNode;

typedef struct node2_def
{
  int blockID;
  unsigned int useTime;

} Node2;

//Data structure which will hold a disk block
typedef struct block_def
{
  int id;
  char * blockData;
  int dirtyBit;
  unsigned int useTime;
  pthread_mutex_t lock;
} block;

typedef struct cache_queue_def
{
  int queueSize;
  int maxQueueSize;
  Node2 * priorityQueue;
} cacheQueue;

/*
Node * cacheMap = (Node *)malloc(sizeof(Node) * cacheSize);
*/
//Node ** cacheMap = (Node **)malloc(sizeof(Node*) * cacheSize);
Node ** cacheMap;
diskNode * diskQueue;
cacheQueue nodeQueue;
//The cache is stored as an array of blocks
// this allocates the cache dynamically
/*
block * fileBufferCache = (block *)malloc(sizeof(block) * cacheSize);
block * diskBlocks = (block *)malloc(sizeof(block) * blocksOnDisk);
*/
block * fileBufferCache;
block * diskBlocks;


//Returns a hash of the int val that is taken in a a parameter.
int mapHash(int val)
{
  int hashVal = (val * 334791) % cacheSize;
  return hashVal;
}

void initializeMap()
{
  cacheMap = (Node **)malloc(sizeof(Node*) * cacheSize);
  int i;
  //Fill the cache with NULL nodes
  for(i = 0; i < cacheSize; i++)
  {
    cacheMap[i] = NULL;
  }
  //Initialize all blocks to not on disk
  for(i = 0; i < blocksOnDisk; i++)
  {
    int hashNum = mapHash(i);
    Node * insNode = (Node *)malloc(sizeof(Node));
    insNode->blockID = i;
    insNode->loc = NOT_FOUND;
    insNode->next = cacheMap[hashNum];
    pthread_mutex_init(&insNode->blockLock, NULL);
    cacheMap[hashNum] = insNode;
  }
}

void initializeQueue()
{
  nodeQueue.maxQueueSize = cacheSize;
  nodeQueue.priorityQueue = (Node2*)malloc(sizeof(Node2) * nodeQueue.maxQueueSize);
  nodeQueue.queueSize = 0;
  int i;
  //Fill the queue with nothing
  for(i = 0; i < nodeQueue.maxQueueSize; i++)
  {
    nodeQueue.priorityQueue[i].blockID = NOT_FOUND;
    nodeQueue.priorityQueue[i].useTime = 0;
  }
}

void initializeCache()
{
  fileBufferCache = (block *)malloc(sizeof(block) * cacheSize);
  int i;
  //Initializes all of the blocks in the cache to empty.
  for(i = 0; i < cacheSize; i++)
  {
    fileBufferCache[i].id = NOT_FOUND;
    fileBufferCache[i].blockData = (char *)malloc(sizeof(char) * diskBlockSize);
    fileBufferCache[i].dirtyBit = NOT_DIRTY;
    fileBufferCache[i].useTime = NOT_FOUND;
    pthread_mutex_init(&fileBufferCache[i].lock, NULL);

  }
}

void initializeDisk()
{
  pthread_mutex_init(&diskLock, NULL);
  pthread_mutex_init(&diskQueueLock, NULL);
  diskQueue = NULL;
  diskBlocks = (block *)malloc(sizeof(block) * blocksOnDisk);
  int i;
  int j;
  for(i = 0; i < blocksOnDisk; i++)
  {
    diskBlocks[i].blockData = (char *)malloc(sizeof(char) * diskBlockSize);
    pthread_mutex_init(&diskBlocks[i].lock, NULL);
    for(j = 0; j < diskBlockSize; j++)
    {
      diskBlocks[i].blockData[j] = 'a' + (i % 26);
    }
  }
}


void destroyNode(Node * toDestroy)
{
  pthread_mutex_destroy(&toDestroy->blockLock);
  free(toDestroy);
}

void destroyDiskNode(diskNode * toDelete)
{
  pthread_cond_destroy(&toDelete->nodeCond);
  free(toDelete);
  return;
}

void siftDown(int i)
{
  //If it is at the end. We are done here
  if(i >= nodeQueue.queueSize)
  {
    return;
  }
  //Otherwise, se if the heap property is violated.
  else
  {
    int leftChild = 2 * i + 1;
    int rightChild = 2 * i + 2;
    int minChild = leftChild;
    //Determines which child is the minChild
    if(nodeQueue.priorityQueue[rightChild].useTime < nodeQueue.priorityQueue[leftChild].useTime)
    {
      //If true, then the rightChild is the minChild
      minChild = rightChild;
    }
    //Determines if a switch is needed
    if(nodeQueue.priorityQueue[i].useTime > nodeQueue.priorityQueue[minChild].useTime)
    {
      //Prepares for the swap
      int tempBlockID = nodeQueue.priorityQueue[i].blockID;
      int tempUseTime = nodeQueue.priorityQueue[i].useTime;
      //Makes the swap
      nodeQueue.priorityQueue[i].blockID = nodeQueue.priorityQueue[minChild].blockID;
      nodeQueue.priorityQueue[i].useTime = nodeQueue.priorityQueue[minChild].useTime;
      nodeQueue.priorityQueue[minChild].blockID = tempBlockID;
      nodeQueue.priorityQueue[minChild].useTime = tempUseTime;
      //Recursive call to continue shifting until located in the correct spot.
      siftDown(minChild);
    }
  }
}

void siftUp(int i)
{
  //If it is at the end. We are done here
  if(i == 0)
  {
    return;
  }
  //Otherwise, se if the heap property is violated.
  else
  {
    int parent = i / 2;
    //Determines if a switch is needed
    if(nodeQueue.priorityQueue[parent].useTime > nodeQueue.priorityQueue[i].useTime)
    {
      //Prepares for the swap
      int tempBlockID = nodeQueue.priorityQueue[i].blockID;
      int tempUseTime = nodeQueue.priorityQueue[i].useTime;
      //Makes the swap
      nodeQueue.priorityQueue[i].blockID = nodeQueue.priorityQueue[parent].blockID;
      nodeQueue.priorityQueue[i].useTime = nodeQueue.priorityQueue[parent].useTime;
      nodeQueue.priorityQueue[parent].blockID = tempBlockID;
      nodeQueue.priorityQueue[parent].useTime = tempUseTime;
      //Recursive call to continue shifting until located in the correct spot.
      siftUp(parent);
    }
  }
}

void removeMinFromQueue(int newBlockID, int newUseTime)
{
  //Enter the critical section
  pthread_mutex_lock(&queueLock);
  //Switch the last element into the current location.
  nodeQueue.priorityQueue[0].blockID = newBlockID;
  nodeQueue.priorityQueue[0].useTime = newUseTime;
  //Fix the map.
  siftDown(0);
  //Exit the critical section
  pthread_mutex_unlock(&queueLock);

}

//Returns the index of blockID in the queue, returns NOT_FOUND if not in queue.
int findInQueue(int blockID, int i, int lookTime)
{
  //Returns NOT_FOUND if the entire queue was checked.
  if(i > nodeQueue.queueSize)
  {
    return NOT_FOUND;
  }
  //If it has been found, return the current index.
  if(nodeQueue.priorityQueue[i].blockID == blockID)
  {
    return i;
  }
  int leftChild = 2 * i + 1;
  int rightChild = 2 * i + 2;
  int storeVal = NOT_FOUND;
  //If the left child is less than or equal to, recurse.
  if(nodeQueue.priorityQueue[leftChild].useTime <= lookTime)
  {
    storeVal = findInQueue(blockID, leftChild, lookTime);
  }
  //If it has not yet been found
  if(storeVal != -1)
  {
    //If the right child is less than or equal to, recurse.
    if(nodeQueue.priorityQueue[rightChild].useTime <= lookTime)
    {
      storeVal = findInQueue(blockID, rightChild, lookTime);
    }
  }
  return storeVal;
}

//Updates the key of the node for nodeID in the queue
void updateQueue(int nodeID, int nodeTime, int lastUseTime)
{
  //Enter the critical section
  pthread_mutex_lock(&queueLock);
  //Finds the node in the queue if it exists.
  int i = findInQueue(nodeID, 0, lastUseTime);
  //If the block was not in the queue, insert it into the queue.
  if(i == NOT_FOUND)
  {
    nodeQueue.priorityQueue[nodeQueue.queueSize + 1].blockID = nodeID;
    nodeQueue.priorityQueue[nodeQueue.queueSize + 1].useTime = nodeTime;
    nodeQueue.queueSize++;
    siftUp(nodeQueue.queueSize + 1);
  }
  //Otherwise, update the key of the correct node
  else
  {
    nodeQueue.priorityQueue[i].useTime = nodeTime;
    siftUp(i);
    siftDown(i);
  }
  //Enter the critical section
  pthread_mutex_unlock(&queueLock);
}

//Finds if a given block is in the cache, if it is then the lock for that
//cache block is locked and the index of the block in the cache is returned.
//If not, the value NOT_FOUND is returned.
//NOTICE THAT THIS RETURNS WITHOUT UNLOCKING TO ENSURE THAT THE CALLING THREAD
//GAINS CONTROL OF THE CACHE BLOCK.
int lookUpInMap(int blocknum)
{
  int hashVal = mapHash(blocknum);
  int returnVal = NOT_FOUND;
  Node * check = cacheMap[hashVal];

  //Find blocknum in the map
  while(check != NULL && check->blockID != blocknum)
  {
    check = check->next;
  }
  //Only check the cache if the blocknum is in the map
  if(check != NULL)
  {
    //Locks the map entry for that block
    pthread_mutex_lock(&check->blockLock);
    returnVal = check->loc;
    //Enter the critical section if the block is in the cache
    if(returnVal != NOT_FOUND)
    {
      //Enter critical section of cache block
      pthread_mutex_lock(&fileBufferCache[returnVal].lock);
    }
  }

  //Note, at this point we still own the lock for the cache block if it exists,
  // as well as the lock of the map entry for the block
  return returnVal;
}


//Removes a block from the map if it exists in the map.
void removeFromMap(int blocknum)
{
  int hashVal = mapHash(blocknum);
  Node * check = cacheMap[hashVal];

  //If there is nothing where it would be, return
  if(check == NULL)
  {
    return;
  }

  //Find the node in the map
  while(check != NULL && check->blockID != blocknum)
  {
    //Move check to the next node
    check = check->next;
  }

  //Sees if the edge of the map was reached without finding it,
  if(check == NULL)
  {
    return;
  }

  //Resets that map location to map the block to not found.
  check->loc = NOT_FOUND;
  //Exits the critical section of that map block
  pthread_mutex_unlock(&check->blockLock);
  return;
}

//Inserts a node containg the block and its index in the cache into the map
void insertIntoMap(int blocknum, int blockLoc)
{
  int hashVal = mapHash(blocknum);
  Node * check = cacheMap[hashVal];
  //Find the block in the map
  while(check->blockID != blocknum)
  {
    check = check->next;
  }
  //Set the map to map the blocknum to blockLock
  check->loc = blockLoc;
  return;

}

//Releases the lock of the given blocknum in the map
void openMapBlock(int blocknum)
{
  int hashVal = mapHash(blocknum);
  Node * check = cacheMap[hashVal];

  //Find blocknum in the map
  while(check != NULL && check->blockID != blocknum)
  {
    check = check->next;
  }
  pthread_mutex_unlock(&check->blockLock);
}



//Uses LRU to decide what cache block to replace, return the index of the cache
//which has been selected. Note that upon returning, the lock for that cache
//location is still locked to ensure that the calling thread now has exclusive
//access to said location.

int chooseRemove(int blocknum)
{
  int returnVal = NOT_FOUND;
  int replaceBlockID;
  int checkTime;
  //Enter critical section
  pthread_mutex_lock(&useLock);
  int storeUse = currUse;
  currUse++;
  //Exit critical section
  pthread_mutex_unlock(&useLock);

  //enter the critical section
  pthread_mutex_lock(&queueLock);
  //If the cache has not yet been filled up, choose the next available cache
  //spot.
  if(numMisses < cacheSize)
  {
    returnVal = numMisses;
    numMisses++;
    printf("MISSED\n");
    nodeQueue.priorityQueue[returnVal].blockID = blocknum;
    nodeQueue.priorityQueue[returnVal].useTime = storeUse;
    //Done with the queue for the moment
    pthread_mutex_unlock(&queueLock);
    //Enter the critical section of that block
    pthread_mutex_lock(&fileBufferCache[returnVal].lock);
    fileBufferCache[returnVal].useTime = storeUse;
  }
  //If the queue has been filled, use LRU to select a cache location
  else
  {
    //Selects the current least revently used node
    replaceBlockID = nodeQueue.priorityQueue[0].blockID;
    checkTime = nodeQueue.priorityQueue[0].useTime;
    //Done with the queue for now
    pthread_mutex_unlock(&queueLock);
    //Captures the map slot so that we can see if it has been used since this
    //process began.
    returnVal = lookUpInMap(replaceBlockID);
    //Repeat this until the cache spot is actually the LRU slot
    while(checkTime != fileBufferCache[returnVal].useTime)
     {
       //Release cotnrol of the cache spot and the map block
       pthread_mutex_unlock(&fileBufferCache[returnVal].lock);
       openMapBlock(returnVal);
       //Enter critical section of the queue
       pthread_mutex_lock(&queueLock);
       //Selects the current least recently used node
       replaceBlockID = nodeQueue.priorityQueue[0].blockID;
       checkTime = nodeQueue.priorityQueue[0].useTime;
       pthread_mutex_unlock(&queueLock);
       //Captures the map slot so that we can see if it has been used since this
       //process began.
       returnVal = lookUpInMap(replaceBlockID);
     }
     //Now remove the block that has been chosen from the map.
     removeFromMap(replaceBlockID);
     //Release control of that map block
     //openMapBlock(replaceBlockID);


     fileBufferCache[returnVal].useTime = storeUse;
     pthread_mutex_lock(&queueLock);
     //Now that we successfully have gained control of the LRU cache block,
     //which was in the first index of the priorityQueue, we must remove that
     //piece from the queue.
     removeMinFromQueue(blocknum, storeUse);
     //Exit the critical section
     pthread_mutex_unlock(&queueLock);
  }

  insertIntoMap(blocknum, returnVal);
  return returnVal;
}

void diskblockread(char *x, int blocknum)
{
  pthread_mutex_lock(&useLock);
  numDiskReads++;
  pthread_mutex_unlock(&useLock);
  //Enter critical section for placing into the FIFO queue
  pthread_mutex_lock(&diskQueueLock);
  diskNode * insNode = (diskNode *)malloc(sizeof(diskNode));
  insNode->blockID = blocknum;
  pthread_cond_init(&insNode->nodeCond, NULL);
  insNode->next = NULL;
  diskNode * checkNode = diskQueue;
  //If the queue is empty, place at the head.
  if(checkNode == NULL)
  {
    diskQueue = insNode;
  }
  //Otherwise put it at the end
  else
  {
    //Find the current last node
    while(checkNode->next != NULL)
    {
      checkNode = checkNode->next;
    }
    //Insert the new node
    checkNode->next = insNode;
  }
  //Exit critical section for the queue
  pthread_mutex_unlock(&diskQueueLock);
  //Enter critical section for the read/write
  pthread_mutex_lock(&diskLock);
  //Wait until you are at the head of the queue
  while(insNode->blockID != diskQueue->blockID)
  {
    //Wait
    pthread_cond_wait(&insNode->nodeCond, &diskLock);
  }
  int i;
  //Read the data from the disk
  for(i = 0; i < diskBlockSize; i++)
  {
    x[i] = diskBlocks[blocknum].blockData[i];
  }
  //Simulate a 1 ms delay.
  usleep(1000);
  //Enter critical section for the queue
  pthread_mutex_lock(&diskQueueLock);
  //Remove the node from the diskQueue
  diskQueue = diskQueue->next;
  //Check for more disk requests
  if(diskQueue != NULL)
  {
    //Signal the next disk request to proceed
    pthread_cond_signal(&diskQueue->nodeCond);
  }
  //Exit critical section of the disk queue
  pthread_mutex_unlock(&diskQueueLock);
  //Exit the critical section of the read/write
  pthread_mutex_unlock(&diskLock);
  //Destroy the disk request that just went through.
  destroyDiskNode(insNode);
  return;
}

void diskblockwrite(char *x, int blocknum)
{
  pthread_mutex_lock(&useLock);
  numDiskWrites++;
  pthread_mutex_unlock(&useLock);
  //Enter critical section for placing into the FIFO queue
  pthread_mutex_lock(&diskQueueLock);
  diskNode * insNode = (diskNode *)malloc(sizeof(diskNode));
  insNode->blockID = blocknum;
  pthread_cond_init(&insNode->nodeCond, NULL);
  insNode->next = NULL;
  diskNode * checkNode = diskQueue;
  //If the queue is empty, place at the head.
  if(checkNode == NULL)
  {
    diskQueue = insNode;
  }
  //Otherwise put it at the end
  else
  {
    //Find the current last node
    while(checkNode->next != NULL)
    {
      checkNode = checkNode->next;
    }
    //Insert the new node
    checkNode->next = insNode;
  }
  //Exit critical section for the queue
  pthread_mutex_unlock(&diskQueueLock);
  //Enter critical section for the read/write

  pthread_mutex_lock(&diskLock);
  //Wait until you are at the head of the queue
  while(insNode->blockID != diskQueue->blockID)
  {
    //Wait for your turn
    pthread_cond_wait(&insNode->nodeCond, &diskLock);
  }
  int i;
  //Write the data to the disk
  for(i = 0; i < diskBlockSize; i++)
  {
    diskBlocks[blocknum].blockData[i] = x[i];
  }
  //Simulate a 1 ms delay.
  usleep(1000);
  //Enter critical section for the queue
  pthread_mutex_lock(&diskQueueLock);
  //Remove the node from the diskQueue
  diskQueue = diskQueue->next;
  //Check for more disk requests
  if(diskQueue != NULL)
  {
    //Signal the next disk request to proceed
    pthread_cond_signal(&diskQueue->nodeCond);
  }
  //Exit critical section of the disk queue
  pthread_mutex_unlock(&diskQueueLock);
  //Exit the critical section of the read/write
  pthread_mutex_unlock(&diskLock);
  //Destroy the disk request that just went through.
  destroyDiskNode(insNode);
  return;
}


//Places the asked for block in the cache at a locations chosen using LRU when
//the cache is full.
//NOTICE THAT THIS RETURNS WITHOUT UNLOCKING TO ENSURE THAT THE CALLING THREAD
//GAINS CONTROL OF THE CACHE BLOCK.
int placeInCache(int blocknum)
{
  //Choose the place to replace
  int replaceLoc = chooseRemove(blocknum);
  //If this block has been written to, write it to memory
  if(fileBufferCache[replaceLoc].dirtyBit == DIRTY)
  {
    //Write it to the disk.
    diskblockwrite(fileBufferCache[replaceLoc].blockData, fileBufferCache[replaceLoc].id);
  }
  //Replace the block chosen with the block requested
  diskblockread(fileBufferCache[replaceLoc].blockData, blocknum);
  return replaceLoc;
}

//NEED TO UPDATE THE MAP IN PLACEINCACHE



void blockread(char *x, int blocknum)
{
  pthread_mutex_lock(&useLock);
  numBlockReads++;
  pthread_mutex_unlock(&useLock);
  int i;
  int blockLoc = NOT_FOUND;

  //Use the map to find the index of the block in the cache, if it is in the
  //cache, we now hold the lock for that block of the cache
  blockLoc = lookUpInMap(blocknum);

  //If the block is not in the cache it must be read from disk
  if(blockLoc == NOT_FOUND)
  {
    //Place the block into the cache
    blockLoc = placeInCache(blocknum);
    printf("The lock for %d has been obtained\n", blockLoc);
  }
  //Update the use time in the queue if it was already in the cache
  else
  {
    printf("The lock for %d has been obtained\n", blockLoc);

    //Enter critical section
    pthread_mutex_lock(&useLock);
    int storeUse = currUse;
    currUse++;
    //Exit critical section
    pthread_mutex_unlock(&useLock);
    //Update the block in the queue
    updateQueue(blocknum, storeUse, fileBufferCache[blockLoc].useTime);
    fileBufferCache[blockLoc].useTime = storeUse;
  }
  //Read from the block in the cache into x.
  for(i = 0; i < diskBlockSize; i++)
  {
      x[i] = fileBufferCache[blockLoc].blockData[i];
    }
  //Exit critical section of cache
  pthread_mutex_unlock(&fileBufferCache[blockLoc].lock);
  printf("The lock for %d was released\n", blockLoc);
  //Exit critical section of map block
  openMapBlock(blocknum);
}



void blockwrite(char *x, int blocknum)
{
  pthread_mutex_lock(&useLock);
  numBlockWrites++;
  pthread_mutex_unlock(&useLock);
  int blockLoc = -1;
  int i;
  //Use the map to find the index of the block in the cache, if it is in the
  //cache, we now hold the lock for that block of the cache
  blockLoc = lookUpInMap(blocknum);

  //If it is not in the cache, it must be read from the disk
  if(blockLoc == NOT_FOUND)
  {
    //Place the block into the cache
    blockLoc = placeInCache(blocknum);
  }
  //Update the use time in the queue if it was already in the cache
  else
  {
    //Enter critical section
    pthread_mutex_lock(&useLock);
    int storeUse = currUse;
    currUse++;
    //Exit critical section
    pthread_mutex_unlock(&useLock);
    //Update the block in the queue
    updateQueue(blocknum, storeUse, fileBufferCache[blockLoc].useTime);
    fileBufferCache[blockLoc].useTime = storeUse;
  }
  //Write to the block from x
  for(i = 0; i < diskBlockSize; i++)
  {
      fileBufferCache[blockLoc].blockData[i] = x[i];
  }
  fileBufferCache[blockLoc].dirtyBit = DIRTY;
  //Exit critical section of cache
  pthread_mutex_unlock(&fileBufferCache[blockLoc].lock);
  //Exit critical section of map block
  openMapBlock(blocknum);
}

void tearDown()
{
  int i;

  pthread_mutex_destroy(&useLock);

  //Destroy the disk
  pthread_mutex_destroy(&diskLock);
  for(i = 0; i < blocksOnDisk; i++)
  {
    pthread_mutex_destroy(&diskBlocks[i].lock);
    free(diskBlocks[i].blockData);
  }
  free(diskBlocks);
  pthread_mutex_destroy(&diskQueueLock);
  //Destroy the diskQueue
  while(diskQueue != NULL)
  {
    diskNode * toDelete = diskQueue;
    diskQueue = diskQueue->next;
    destroyDiskNode(toDelete);
  }

  //Destroy the map
  for(i = 0; i < cacheSize; i++)
  {
    while(cacheMap[i] != NULL)
    {
      Node * toDestroy = cacheMap[i];
      cacheMap[i] = cacheMap[i]->next;
      destroyNode(toDestroy);
    }
  }
  //Destroy the cacheQueue
  pthread_mutex_destroy(&queueLock);
  free(nodeQueue.priorityQueue);
  //Destroy the cache
  for(i = 0; i < cacheSize; i++)
  {
    pthread_mutex_destroy(&fileBufferCache[i].lock);
    free(fileBufferCache[i].blockData);
  }
  free(fileBufferCache);
}

//Sequentially accesses NUM_BLOCKS_SEQUENTIAL on the disk, starting at a
//random block. accessType is READ_ACCESS for a read and WRITE_ACCESS for a write
void sequentialAccess(int accessType)
{
  //Generates the random block to start from.
  int startBlock = 0;
  int x;

  //Performs the reads if asked to
  if(accessType == READ_ACCESS)
  {
    //Generate somewhere to store the read data.
    char ** storeData = (char **)malloc(sizeof(char *) * NUM_BLOCKS_SEQUENTIAL);
    for(x = 0; x < NUM_BLOCKS_SEQUENTIAL; x++)
    {
      storeData[x] = (char *)malloc(sizeof(char) * diskBlockSize);
    }
    //Accesses the blocks
    for(x = 0; x < NUM_BLOCKS_SEQUENTIAL; x++)
    {
      int accessBlock = startBlock + x;
      blockread(storeData[x], accessBlock);
      /*
      int j;
      for(j = 0; j < diskBlockSize; j++)
      {
        printf("%c", storeData[x][j]);
      }
      printf("\n");
      */
    }
  }
  //Performs the writes if asked to
  if(accessType == WRITE_ACCESS)
  {
    //Accesses the blocks
    for(x = 0; x < NUM_BLOCKS_RANDOM; x++)
    {

    }
  }

}

void randomAccess(int accessType)
{

}

//NEED TO IMPELEMENT TWO DIFFERENT KINDS OF THREAD, SEQUENTIAL AND RANDOM
//EACH THREAD NEEDS TO READ OR WRITE FROM THE CACHE.
void *worker(void *args)
{
  //Name of the thread
	int threadId =*((int *) args);
  //This is the type of thread being created. It will either be
  //SEQUENTIAL or RANDOM
  int threadType = *(((int *) args) + 1);
  printf("Beginning thread %d which is of type %d\n", threadId, threadType);
  if(threadType == SEQUENTIAL)
  {
    sequentialAccess(READ_ACCESS);
  }
  if(threadType == RANDOM)
  {
    randomAccess(READ_ACCESS);
  }
  printf("Exiting thread %d now\n", threadId);
  pthread_exit(NULL);
}

int main()
{
  initializeMap();
  initializeDisk();
  initializeCache();
  initializeQueue();
  pthread_t threads[40];
	int i;
	int rc;
	int args[2][2]=		//pairs of thread id and access type
		{{0,SEQUENTIAL},{1,SEQUENTIAL}};

	for(i = 0; i < 2; i++)
	{
		rc = pthread_create(&threads[i], NULL, &worker, (void *)(args[i]));
		if(rc)
		{
			printf("** could not create thread %d\n",i); exit(-1);
		}
	}

	for(i = 0; i < 2; i++)
	{
		rc = pthread_join(threads[i], NULL);
		if(rc)
		{
			printf("** could not join thread %d\n", i); exit(-1);
			tearDown(); return 0;
		}
	}

  tearDown();
  return 0;
}
