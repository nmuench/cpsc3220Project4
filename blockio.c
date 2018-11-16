#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define NOT_DIRTY 0
#define DIRTY 1
#define NOT_FOUND -1

const int diskBlockSize = 1024; //The size of a disk block in bytes
const int blocksOnDisk = 4096; //The number of blocks on the disk
const int timeForIO = 1; //The time in ms for a diskread or diskwrite
const int cacheSize = 64; //The size of the cache in blocks
int replaceNum = 0;
int numBlocksRead = 0;
int currWrite = 0;
pthread_mutex_t mapLock;
pthread_mutex_t queueLock;


typedef struct node_def
{
  int loc;
  int blockID;
  struct node_def * next;
} Node;

typedef struct node2_def
{
  int loc;
  int writeTime;
  struct node2_def * next;
  struct node2_def * prev;

} Node2;

//Data structure which will hold a disk block
typedef struct block_def
{
  int id;
  char * blockData;
  int dirtyBit;
  int writeTime;
  pthread_mutex_t lock;
  /*
  block()
  {
    id = -1;
    blockData = (char *)malloc(sizeof(char) * diskBlockSize);
    dirtyBit = 0;
    placeNum = numBlocksRead;
    pthread_mutex_init(&lock);
    currUse = OPEN;
  }
  block(int blockNum, char *x, int use)
  {
    id = blockNum;
    blockData = (char *)malloc(sizeof(char) * diskBlockSize);
    int i;
    //Copies the data from x into blockData
    for(i = 0; i < diskBlockSize; i++)
    {
      blockData[i] = x[i];
    }
    dirtyBit = 0;
    placeNum = numBlocksRead;
    pthread_mutex_init(&lock);
    currUse = use;
  }
  */
} block;

typedef struct cache_queue_def
{
  Node2 * frontTaken;
  //THE NODE FOR BACK TAKEN HAS A NEXT WHICH ACTUALLY FUNCTIONS AS A PREVIOUS
  Node2 * backTaken;
  Node2 * frontUntaken;
} cacheQueue;

/*
Node * cacheMap = (Node *)malloc(sizeof(Node) * cacheSize);
*/
//Node ** cacheMap = (Node **)malloc(sizeof(Node*) * cacheSize);
Node ** cacheMap;
cacheQueue nodeQueue;
//The cache is stored as an array of blocks
// this allocates the cache dynamically
/*
block * fileBufferCache = (block *)malloc(sizeof(block) * cacheSize);
block * diskBlocks = (block *)malloc(sizeof(block) * blocksOnDisk);
*/
block * fileBufferCache;
block * diskBlocks;
/*
//Prepares the map to be used
void initilizaeMap()
{
  int i;
  //Initialize all cache spots to empty.
  for(i = 0; i < cacheSize; i++)
  {
    cacheMap[i] = Node();
    cacheMap[i].loc = i;
    cacheMap[i].blockID = -1;
    pthread_mutex_init(&cacheMap[i].lock);
  }
}
*/
void initializeMap()
{
  cacheMap = (Node **)malloc(sizeof(Node*) * cacheSize);
  pthread_mutex_init(&mapLock, NULL);
  int i;
  //Initialize all cache spots to empty.
  for(i = 0; i < cacheSize; i++)
  {
    cacheMap[i] = NULL;
  }
}

void initializeQueue()
{
  nodeQueue.frontTaken = NULL;
  nodeQueue.backTaken = NULL;
  int i;
  //Fill the untaken list with all of the queue spots.
  for(i = 0; i < cacheSize; i++)
  {
    Node2 * insNode = (Node2 *)malloc(sizeof(Node2));
    insNode->writeTime = NOT_FOUND;
    insNode->loc = i;
    insNode->next = nodeQueue.frontUntaken;
    nodeQueue.frontTaken->prev = insNode;
    insNode->prev = NULL;
    nodeQueue.frontUntaken = insNode;
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
    fileBufferCache[i].writeTime = NOT_FOUND;
    pthread_mutex_init(&fileBufferCache[i].lock, NULL);

  }
}

void initializeDisk()
{
    block * diskBlocks = (block *)malloc(sizeof(block) * blocksOnDisk);
}

//Returns a hash of the int val that is taken in a a parameter.
int mapHash(int val)
{
  int hashVal = (val * 334791) % cacheSize;
  return hashVal;
}

void destroyNode(Node * toDestroy)
{
  free(toDestroy);
}

//Finds if a given block is in the cache, if it is then the lock for that
//cache block is locked and the index of the block in the cache is returned.
//If not, the value NOT_FOUND is returned.
//NOTICE THAT THIS RETURNS WITHOUT UNLOCKING TO ENSURE THAT THE CALLING THREAD
//GAINS CONTROL OF THE CACHE BLOCK.
int lookUpInMap(int blocknum)
{
  //Enter critical section
  pthread_mutex_lock(&mapLock);
  int hashVal = mapHash(blocknum);
  int returnVal = NOT_FOUND;
  Node * check = cacheMap[hashVal];

  //Find blocknum if it is present
  while(check != NULL && check->blockID != blocknum)
  {
    check = check->next;
  }
  //Only check the cache if the blocknum is in the map
  if(check != NULL)
  {
    returnVal = check->loc;
    //Enter critical section of cache block
    pthread_mutex_lock(&fileBufferCache[returnVal].lock);
    //If the cache block has changed from what was expected,
    //the block isn't in the cache
    if(returnVal != NOT_FOUND && fileBufferCache[returnVal].id != blocknum)
    {
      //Exit the critical section
      pthread_mutex_unlock(&fileBufferCache[returnVal].lock);
      returnVal = NOT_FOUND;
    }
  }
  //Exit the map critical section
  pthread_mutex_unlock(&mapLock);
  //Note, at this point we still own the lock for the cache block if it exists,
  // return the index of said cache block if it exists.
  return returnVal;
}


//Removes a block from the map if it exists in the map.
void removeFromMap(int blocknum)
{
  //Enter critical section
  pthread_mutex_lock(&mapLock);
  int hashVal = mapHash(blocknum);
  Node * check = cacheMap[hashVal];
  Node * prev = check;

  //If there is nothing where it would be, return
  if(check == NULL)
  {
    pthread_mutex_unlock(&mapLock);
    return;
  }
  //If the first node is the correct block, replace it with the next block
  if(check->blockID == blocknum)
  {
    cacheMap[hashVal] = check->next;
    //Exit the critical section
    pthread_mutex_unlock(&mapLock);
    destroyNode(check);
    return;
  }

  //Otherwise go and find the node in the map
  while(check != NULL && check->blockID != blocknum)
  {
    //Set prev to point at current node
    prev = check;
    //Move check to the next node
    check = check->next;
  }

  //Sees if the edge of the map was reached without finding it,
  if(check == NULL)
  {
    //Exit the critical section
    pthread_mutex_unlock(&mapLock);
    return;
  }

  //If check does exist, it must be the node to delete
  prev->next = check->next;
  //Exit the critical section
  pthread_mutex_unlock(&mapLock);
  destroyNode(check);
  return;

}

//Inserts a node containg the block and its index in the cache into the map
void insertIntoMap(int blocknum, int blockLoc)
{
  //Enter critical section
  pthread_mutex_lock(&mapLock);
  int hashVal = mapHash(blocknum);
  Node * insNode = (Node *)malloc(sizeof(Node));
  insNode->blockID = blocknum;
  insNode->loc = blockLoc;
  insNode->next = cacheMap[hashVal];
  cacheMap[hashVal] = insNode;

  //Exit the critical section
  pthread_mutex_unlock(&mapLock);
  return;

}

//Uses LRU to decide what cache block to replace, return the Node2 which holds
//the relevant information: index in cache and writeTime.
Node2 * chooseRemove()
{
  int returnVal = NOT_FOUND;
  Node2 * locNode;
  //enter the critical section
  pthread_mutex_lock(&queueLock);
  //If the queue does not contain a node for every cache slot, fill up the next
  //empty cache slot
  if(nodeQueue.frontUntaken != NULL)
  {
    locNode = nodeQueue.frontUntaken;
    //Remove the cache slot to be filled from the untaken list
    nodeQueue.frontUntaken = nodeQueue.frontUntaken->next;
    nodeQueue.frontUntaken->prev = NULL;
  }
  //Otherwise, take the least recently used node from the taken list.
  else
  {
    locNode = nodeQueue.backTaken;
    //Remove the cache slot to be filled from the untaken list
    //Update the back of taken
    nodeQueue.backTaken = nodeQueue.backTaken->prev;
    nodeQueue.backTaken->next = NULL;
  }

  //Exit the critical section
  pthread_mutex_unlock(&queueLock);
  return locNode;
}


void diskblockread(char *x, int blocknum)
{

}

void diskblockwrite(char *x, int blocknum)
{

}

//Places the asked for block in the cache at a locations chosen using LRU when
//the cache is full.
//NOTICE THAT THIS RETURNS WITHOUT UNLOCKING TO ENSURE THAT THE CALLING THREAD
//GAINS CONTROL OF THE CACHE BLOCK.
int placeInCache(int blocknum)
{
  //Choose the place to replace
  Node2 *replaceNode = chooseRemove();
  int replaceLoc = replaceNode->loc;
  //Enter critical section
  pthread_mutex_lock(&fileBufferCache[replaceLoc].lock);
  //Continue using LRU to select until we get a block that wasn't written to after
  //Our check.
  while(fileBufferCache[replaceLoc].writeTime != replaceNode->writeTime)
  {
    //Unlock the previous locations
    pthread_mutex_unlock(&fileBufferCache[replaceLoc].lock);
    Node2 *replaceNode = chooseRemove();
    int replaceLoc = replaceNode->loc;
    //Obtain the lock for the new location.
    pthread_mutex_lock(&fileBufferCache[replaceLoc].lock);
  }
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


void blockread(char *x, int blocknum)
{
  int i;
  int blockLoc = NOT_FOUND;

  //Use the map to find the index of the block in the cache
  blockLoc = lookUpInMap(blocknum);

  //If the block is not in the cache it must be read from disk
  if(blockLoc == NOT_FOUND)
  {
    //Place the block into the cache
    blockLoc = placeInCache(blocknum);
  }
  //Read from the block in the cache into x.
  for(i = 0; i < diskBlockSize; i++)
  {
      x[i] = fileBufferCache[blockLoc].blockData[i];
    }
  //Exit critical section
  pthread_mutex_unlock(&fileBufferCache[blockLoc].lock);
}



void blockwrite(char *x, int blocknum)
{
  int blockLoc = -1;
  int i;
  blockLoc = lookUpInMap(blocknum);

  //If it is not in the cache, it must be read from the disk
  if(blockLoc == NOT_FOUND)
  {
    //Place the block into the cache
    blockLoc = placeInCache(blocknum);
  }

  //Enter critical section.
  pthread_mutex_lock(&fileBufferCache[blockLoc].lock);
  //Write to the block from x
  for(i = 0; i < diskBlockSize; i++)
  {
      fileBufferCache[blockLoc].blockData[i] = x[i];
  }
  fileBufferCache[blockLoc].dirtyBit = 1;
  fileBufferCache[blockLoc].writeTime = currWrite;
  currWrite++;
  //Exit critical section
  pthread_mutex_unlock(&fileBufferCache[blockLoc].lock);

}


//PREP FOR CASE OF MULTIPLE READS AND WRITES ASKING FOR SAME BLOCK WHICH ISNT IN CACHE YET.
//NEED TO ENSURE CONCURRENT READS/WRITES ARE OK.
