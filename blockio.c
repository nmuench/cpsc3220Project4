#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define FOR_READ 1
#define FOR_WRITE 0
#define OPEN -1
#define NOT_FOUND -1

const int diskBlockSize = 1024; //The size of a disk block in bytes
const int blocksOnDisk = 4096; //The number of blocks on the disk
const int timeForIO = 1; //The time in ms for a diskread or diskwrite
const int cacheSize = 64; //The size of the cache in blocks
int replaceNum = 0;
int numBlocksRead = 0;
pthread_mutex_t insertLock;

/*
struct Node
{
  int loc;
  int blockID;
  pthread_mutex_t lock;
  Node(){id = -1; index = -1; pthread_mutex_init(&lock);}
};
*/
struct Node
{
  int loc;
  int blockID;
  pthread_mutex_t lock;
  Node * next;
  Node(){id = -1; index = -1; pthread_mutex_init(&lock); next = NULL;}
};

//Data structure which will hold a disk block
struct block
{
  int id;
  char * blockData;
  int placeNum;
  int dirtyBit;
  pthread_mutex_t lock;
  int currUse;
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
};

/*
Node * cacheMap = (Node *)malloc(sizeof(Node) * cacheSize);
*/
Node ** cacheMap = (Node **)malloc(sizeof(Node*) * cacheSize);

//The cache is stored as an array of blocks
// this allocates the cache dynamically
block * fileBufferCache = (block *)malloc(sizeof(block) * cacheSize);
block * diskBlocks = (block *)malloc(sizeof(block) * blocksOnDisk);

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
void initilizaeMap()
{
  int i;
  //Initialize all cache spots to empty.
  for(i = 0; i < cacheSize; i++)
  {
    cacheMap[i] = (Node *)malloc(sizeof(Node));
    cacheMap[i]->blockID = -1;
    cacheMap[i]->loc = -1;
    cacheMap[i]->next = NULL;
    pthread_mutex_init(&cacheMap[i]->lock, NULL);
  }
}

//Finds if a given block is in the cache, if it is then the lock for that
//cache block is locked and the index of the block in the cache is returned.
//If not, the value NOT_FOUND is returned.
int findInMap(int blocknum)
{
  int hashVal = mapHash(blocknum);
  Node * check = cacheMap[hashVal];
  pthread_mutex_lock(&check->lock);
  //Find blocknum if it is present
  while(check->next != NULL && check->blockID != blocknum)
  {
    Node * prev = check;
    check = check->next;
    //Release the node you have already checked.
    pthread_mutex_unlock(&prev->lock)
    //Capture the lock of the node to be checked
    pthread_mutex_lock(&check->lock);
  }
  //If it was not present, release the lock and return NOT_FOUND
  if(check->blockID != blocknum)
  {
    pthread_mutex_unlock(&check->lock);
    return NOT_FOUND;
  }
  //Capture the lock of the block that holds blocknum's data in the cache
  pthread_mutex_lock(&fileBufferCache[check->loc].lock);
  //If this is no longer the requested block, we must search again to see if
  //it was added in a new location, hence the recursive call.
  if(fileBufferCache[check->loc].id != blocknum)
  {
    pthread_mutex_unlock(&check->lock);
    return findInMap(blocknum);
  }
  //If the block has been successfully found in the cache, take control of its
  //lock and return its index in the cache so that the calling thread can use it
  //without the possibility of a race condition.
  pthread_mutex_unlock(&check->lock);
  return check->loc;

}

void removeFromMap(int blocknum)
{
  int hashVal = mapHash(blocknum);
  Node * check = cacheMap[hashVal];
  pthread_mutex_lock(&check->lock);
}

void initializeCache()
{

}

void initializeDisk()
{

}

int placeInCache(int blocknum)
{
  //Enter critical section
  pthread_mutex_lock(&insertLock);
  //If this block has been written to, write it to memory
  fileBufferCache[replaceLoc]->
    //Enter critical section
  //Replace the block chosen with the block requested
    //Enter critical section
}

int checkCache(int blocknum)
{
  int i = 0;
  //Check to see if the block is stored in the cache.
  while(i < cacheSize && blockLoc == NOT_FOUND)
  {
    pthread_mutex_lock(&cacheMap[i].lock);
    //If the block is found, save its location.
    if(cacheMap[i].blockID == blocknum)
    {
      //Capture the lock for this block
      pthread_mutex_lock(&fileBufferCache[i].lock);
      //See if it still contains the block
      if(fileBufferCache[i].id == blocknum)
      {
        return i;
      }
      pthread_mutex_unlock(&fileBufferCache[i].lock);
      return NOT_FOUND;
    }
    pthread_mutex_unlock(&cacheMap[i].lock);
    i++;
  }
  return NOT_FOUND;
}

void blockread(char *x, int blocknum)
{
  int blockLoc = -1;
  blockLoc = checkCache(blocknum);

  //If the block is not in the cache it must be read from disk
  if(blockLoc == NOT_FOUND)
  {
    //Place the block into the cache
    blockLoc = placeInCache(blocknum);
  }
  //Enter critical section.
  pthread_mutex_lock(&fileBufferCache[blockLoc].lock);
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
  blockLoc = checkCache(blocknum);

  //If it is not in the cache, it must be read from the disk
  if(blockLoc == NOT_FOUND)
  {
    //Place the block into the cache
    blockLoc = placeInCache(blocknum);
  }

  //Enter critical section.
  pthread_mutex_lock(&fileBufferCache[blockLoc].lock);
  //RWrite to the block from x
  for(i = 0; i < diskBlockSize; i++)
  {
      fileBufferCache[blockLoc].blockData[i] = x[i];
  }
  fileBufferCache[blockLoc].dirtyBit = 1;
  //Exit critical section
  pthread_mutex_unlock(&fileBufferCache[blockLoc].lock);

}


//PREP FOR CASE OF MULTIPLE READS AND WRITES ASKING FOR SAME BLOCK WHICH ISNT IN CACHE YET.
//NEED TO ENSURE CONCURRENT READS/WRITES ARE OK.
