#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <comp421/filesystem.h>
#include <comp421/iolib.h>
#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "yfs.h"

struct blockInfo {
    int dirty;
    int blockNumber;
    struct blockInfo *next;
    struct blockInfo *prev;
    char data[BLOCKSIZE];
};

struct blockWrap {
   int key;
   struct blockInfo* blockData;
};

struct inodeInfo {
    int dirty;
    int inodeNum;
    struct inodeInfo *next;
    struct inodeInfo *prev;
    struct inode *inodeVal;
};

struct inodeWrap {
   int key;
   struct inodeInfo* infoInode;
};

int NUM_INODES;
int NUM_BLOCKS;

short* freeInodesList;
short* freeBlockList;

// Variables for block cache
int currBlockCacheNum;
struct blockInfo* frontBlock;
struct blockInfo* rearBlock;
struct blockWrap* blockHT[BLOCK_CACHESIZE]; 
struct blockWrap* nullBlockWrap;
struct blockInfo* nullBlockInfo;

// Variables for inode cache
int currInodeCacheNum;
struct inodeInfo* frontInode;
struct inodeInfo* rearInode;
struct inodeWrap* inodeHT[INODE_CACHESIZE]; 
struct inodeWrap* nullInodeWrap;
struct inodeInfo* nullInodeInfo;

void dequeueBlock();
void removeQBlock(struct blockInfo* block);
struct blockInfo* getLRUBlock(int blockNum);
void initCache();
void removeBlock(); 
void setLruBlock(int blockNum, struct blockInfo* newBlock);
struct inodeWrap *getInode(int key);
void putInodeHashtable(int key, struct inodeInfo* info); 
void putBlockHashtable(int key, struct blockInfo* info);
struct blockWrap* removeBlockHashtable(int blockNum);
void enqueueBlock(struct blockInfo* block);
void enqueueInode(struct inodeInfo* inode);
void dequeueInode();
void popQueueInode(struct inodeInfo* inode);
struct inodeInfo* getLruInode(int inodeNum);
struct blockInfo* readDiskBlock(int blockNum);
struct inodeInfo* readDiskInode(int inodeNum);
void setLruInode(int inodeNum, struct inodeInfo* newLruInode);
int getBlockFromInode(int inodeNum);
int generateInodeHashcode(int key);
int generateBlockHashcode(int key);
struct blockWrap *getBlock(int key);
int sync();
struct inodeWrap* removeInodeHashtable(int inodeNum);