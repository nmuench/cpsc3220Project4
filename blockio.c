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

struct cacheVars
{
  int cacheSize;
  int blockSize;
  pthread_mutex_t insertLock;
  cacheVars()
  {
    cacheSize = -1;
    blockSize = -1;
    pthread_cond_init(&insertLock);

  }
}

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

//The cache is stored as an array of blocks
// this allocates the cache dynamically
block * fileBufferCache = (block *)malloc(sizeof(block) * cacheSize);
block * diskBlocks = (block *)malloc(sizeof(block) * blocksOnDisk);
aLock * blockLocks = (aLock *)malloc(sizeof(aLock) * blocksOnDisk);


int placeInCache(int blocknum)
{
  //Enter critical section
  pthread_mutex_lock();
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
    //If the block is found, save its location.
    if(fileBufferCache[i]->id == blocknum)
    {
      return i;
    }
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
