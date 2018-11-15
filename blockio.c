#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

const int diskBlockSize = 1024; //The size of a disk block in bytes
const int blocksOnDisk = 4096; //The number of blocks on the disk
const int timeForIO = 1; //The time in ms for a diskread or diskwrite
const int cacheSize = 64; //The size of the cache in blocks

//Data structure which will hold a disk block
struct block
{
  int id;
  char * blockData;
  int dirtyBit;
  block()
  {
    id = -1;
    blockData = (char *)malloc(sizeof(char) * diskBlockSize);
    dirtyBit = 0;
  }
  block(int blockNum, char *x)
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
  }
};

//The cache is stored as an array of blocks
// this allocates the cache dynamically
block * fileBufferCache = (block *)malloc(sizeof(block) * cacheSize);


void blockread(char *x, int blocknum)
{
  int blockLoc = -1;
  int replaceLoc = -1;
  int i = 0;
  //Check to see if the block is stored in the cache.
  while(i < cacheSize && blockLoc == -1)
  {
    //If the block is found, save its location.
    if(fileBufferCache[i]->id = blocknum)
    {
      blockLoc = i;

    }
  }
    //If the block is not in the cache
    if(blockLoc == -1)
    {
      //Use LRU to decide what block to remove from the cache
      replaceLoc = checkLRU();
        //Enter critical section
        while(fileBufferCache[replaceLoc].value != FREE)
        {
          pthread_cond_wait(&fileBufferCache[replaceLoc].cond, &fileBufferCache[replaceLoc].lock);
        }
        //If this block has been written to, write it to memory
        fileBufferCache[replaceLoc]->
          //Enter critical section
        //Replace the block chosen with the block requested
          //Enter critical section
    }

      //Read from the block in the cache into x.
        //Enter critical section
}

void blockwrite(char *x, int blocknum)
{
  //Check to see if the block is in the cache
    //If it is, write to the block
      //Enter critical section
    //If it is not in the cache, use LRU to select a block to replace
      //If this block has been written to, write it back to the disk.
        //Enter critical section
      //Replace the block in the cache, write to the new block
        //Enter critical section
    //Set the dirty bit of the block
      //Enter critical section.


}
