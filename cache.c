#include "yfs.h"
#include "cache.h"

void initCache() {
	nullBlockInfo = (struct blockInfo*) malloc(sizeof(struct blockInfo));
    nullBlockInfo->blockNumber = -1;
	nullBlockWrap = (struct blockWrap*) malloc(sizeof(struct blockWrap));
    nullBlockWrap->key = -1;
    currBlockCacheNum = 0;
	currInodeCacheNum = 0;
    frontBlock = NULL;
    rearBlock = NULL;
    frontInode = NULL;
    rearInode = NULL;
    nullInodeWrap = (struct inodeWrap*) malloc(sizeof(struct inodeWrap));
    nullInodeWrap->key = -1;
    nullInodeInfo = (struct inodeInfo*) malloc(sizeof(struct inodeInfo));
    nullInodeInfo->inodeNum = -1;
    nullInodeWrap->infoInode = nullInodeInfo;

    // initialize all values to null
	TracePrintf(1, "cache: initCache - initializaing all values in hashtables to null\n");
    int i;
    for(i = 0; i < INODE_CACHESIZE; i++) {
		inodeHT[i] = NULL;
    }
    for(i = 0; i < BLOCK_CACHESIZE; i++) {
		blockHT[i] = NULL;
    }
}

int getBlockFromInode(int inodeNum) {
    return 1 + (inodeNum / (BLOCKSIZE / INODESIZE));
}

int generateInodeHashcode(int key) {
    return key % INODE_CACHESIZE;
}

int generateBlockHashcode(int key) {
    return key % BLOCK_CACHESIZE;
}

struct blockWrap *getBlock(int key) {
    int hash = generateBlockHashcode(key);  
    int start = hash;

    while (blockHT[hash] != NULL) {
		if(blockHT[hash]->key == key) {
			return blockHT[hash]; 
		}
		hash++;
		hash = hash%BLOCK_CACHESIZE;

		if(hash == start)
			TracePrintf(1, "cache: getBlock - Reached end of hash table and could not find key\n");
			return NULL;
    }        
    return NULL;        
}

void putBlockHashtable(int key, struct blockInfo* info) {
    struct blockWrap *wrap = (struct blockWrap*) malloc(sizeof(struct blockWrap));
    wrap->blockData = info;
    wrap->key = key;
    int hash = generateBlockHashcode(key);

    while(blockHT[hash] != NULL && blockHT[hash]->key != -1) {
		hash++;
		hash = hash%BLOCK_CACHESIZE;
    }
    blockHT[hash] = wrap;
}

struct blockWrap* removeBlockHashtable(int blockNum) {

    int hash = generateBlockHashcode(blockNum);
    int start = hash;

    while(blockHT[hash] != NULL) {
		if(blockHT[hash]->key == blockNum) {
			struct blockWrap* temp = blockHT[hash]; 
			blockHT[hash] = nullBlockWrap; 
			return temp;
		}
		hash++;
		hash = hash%BLOCK_CACHESIZE;
		// made full circule without finding opening
		if(hash == start) {
			TracePrintf(1, "cache: removeBlockHashtable - Made it all the way hashtable without matching key\n");
			return NULL;
		}
    }      
    return NULL;        
}

void enqueueBlock(struct blockInfo *block) {
    TracePrintf(1, "cache: enqueueBlock - Attempting to add block info %p to queue\n", block);
    if(frontBlock == NULL && rearBlock == NULL) {
		frontBlock = rearBlock =block;
		frontBlock->prev = NULL;
		rearBlock->next = NULL;
		return;
    }
    rearBlock->next =block;
    block->prev = rearBlock;
    rearBlock =block;
    rearBlock->next = NULL;
    frontBlock->prev = NULL;
}

void dequeueBlock() {
    //Eliminate the frontBlock;
	TracePrintf(1, "cache: dequeueBlock - Attempting to remove the front block\n");
    if(frontBlock == NULL) {
		TracePrintf(1, "cache: dequeueBlock - Attempting to remove when queue is empty\n");
		return;
    }
    if(frontBlock == rearBlock) {
		frontBlock = rearBlock = NULL;
    }else {
		frontBlock->next->prev = NULL;
		frontBlock = frontBlock->next;
		frontBlock->prev = NULL;
    }
	TracePrintf(1, "cache: dequeueBlock - Successfully removed the front block\n");
}

void removeQBlock(struct blockInfo *block) {
	// middle of q
    if(block->prev != NULL &&block->next != NULL) {
		block->prev->next =block->next;
		block->next->prev =block->prev;
		block->prev = NULL;
		block->next = NULL;
	// tail of q
    }else if(block->prev != NULL &&block->next == NULL) {
		rearBlock = rearBlock->prev;
		rearBlock->next = NULL;
		block->prev = NULL;
		block->next = NULL;
	//beginning of q
    }else if(block->prev == NULL &&block->next != NULL) {
		dequeueBlock();
		block->prev = NULL;
		block->next = NULL;
    }
	TracePrintf(1, "cache: removeQBlock - Successfuly removed blockInfo %p\n", block);
}

struct blockInfo* getLRUBlock(int blockNum) {
    struct blockWrap* result = getBlock(blockNum);
    if(result == NULL) {
		TracePrintf(1, "cache: getLRUBlock - Result is a null block...\n");
		return nullBlockInfo;
    }else{
		removeQBlock(result->blockData);
		enqueueBlock(result->blockData);
		return result->blockData;
    }
}

int sync() {
	TracePrintf(3, "cache: sync - Writing back to disk. Syncing up...\n");
    struct inodeInfo* tempInode= frontInode;
    struct blockInfo* tempBlock= frontBlock;

    while(tempInode != NULL) {
		int inodeNum = tempInode->inodeNum;
		if(tempInode->dirty == 1) {
			// dirty... we gotta read
			int blockNum_to_write = getBlockFromInode(inodeNum);
			struct blockInfo *temp = readDiskBlock(blockNum_to_write);
			memcpy((void*)(temp->data + (inodeNum - (BLOCKSIZE/INODESIZE) * (blockNum_to_write - 1)) * INODESIZE), (void*)(tempInode->inodeVal), INODESIZE);
			temp->dirty = 1;
			tempInode->dirty = 0;
		}
		tempInode = tempInode->next;
    }
    
    while(tempBlock != NULL) {
		if(tempBlock->dirty == 1) {
			// dirty... we gotta write it back
			int res = WriteSector(tempBlock->blockNumber, (void*)(tempBlock->data));
			if(res == 0) {
				tempBlock->dirty = 0;
			}else{
				TracePrintf(1, "cache: sync - WriteSector call failed\n");
				return ERROR;
			}
			// not dirty anymore because we wrote to disk
			tempBlock->dirty = 0;
		}
		tempBlock = tempBlock->next;
    }
	TracePrintf(1, "cache: sync - Complete\n");
    return 0;
}


void removeBlock() {
    // see if we need to remove a block
    if(currBlockCacheNum >= BLOCK_CACHESIZE) {
		int delKey = frontBlock->blockNumber;
		// write inode back to the disk
		sync();
		if(frontBlock->dirty == 1) {
			//int res = WriteSector(frontBlock->blockNumber, (void*)(frontBlock->data));
			if(WriteSector(frontBlock->blockNumber, (void*)(frontBlock->data)) == 0) {
				TracePrintf(1, "cache: removeBlock - WriteSector call failed\n");
			}
		}
		dequeueBlock();
		removeBlockHashtable(delKey);
		// decrement the current block cache number
		currBlockCacheNum--;
    }
}

void setLruBlock(int blockNum, struct blockInfo* newBlock) {
    if(getBlock(blockNum) == NULL) {
		removeBlock();
		enqueueBlock(newBlock);
		putBlockHashtable(blockNum, newBlock);
		currBlockCacheNum++;
		return;
    }else{
		removeQBlock(getBlock(blockNum)->blockData);
		enqueueBlock(newBlock);
		putBlockHashtable(blockNum, newBlock);
		return;
    }
}


//Inode functions

struct inodeWrap *getInode(int key) {
    int hash = generateInodeHashcode(key);  
    int start = hash;
    while(inodeHT[hash] != NULL) {
		if(inodeHT[hash]->key == key) {
			return inodeHT[hash]; 
		}
		hash++;
		hash = hash % INODE_CACHESIZE;
		
		//made a full circle but couldn't find an opening
		if(hash == start) {
			TracePrintf(1, "cache: getInode - Couldn't find inode in hash\n");
			return NULL;
		}
    }        
    return NULL;        
}

void putInodeHashtable(int key, struct inodeInfo* info) {
    struct inodeWrap *wrap = (struct inodeWrap*) malloc(sizeof(struct inodeWrap));
    wrap->infoInode = info;
    wrap->key = key;
    int hash = generateInodeHashcode(key);

    while(inodeHT[hash] != NULL && inodeHT[hash]->key != -1) {
		hash++;
		hash = hash % INODE_CACHESIZE;
    }
    inodeHT[hash] = wrap;
}

struct inodeWrap* removeInodeHashtable(int inodeNum) {
    int hash = generateInodeHashcode(inodeNum);
    int start = hash;

    while(inodeHT[hash] != NULL) {
		if(inodeHT[hash]->key == inodeNum) {
			struct inodeWrap* temp = inodeHT[hash]; 
			inodeHT[hash] = nullInodeWrap; 
			return temp;
		}
		hash++;
		hash = hash % INODE_CACHESIZE;
		
		if(hash == start) {
			TracePrintf(1, "cache: removeInodeHashtable - Could not find Inode to remove\n");
			return NULL;
		}
    }      
    return NULL;        
}

void enqueueInode(struct inodeInfo *inode) {
    // adds inode to as last member of queue
    if(frontInode == NULL && rearInode == NULL) {
		frontInode = rearInode = inode;
		frontInode->prev = NULL;
		rearInode->next = NULL;
		return;
    }
    rearInode->next = inode;
    inode->prev = rearInode;
    rearInode = inode;
    rearInode->next = NULL;
}


void dequeueInode() {
    // removes the front Inode
    if(frontInode == NULL) {
		printf("Queue is Empty\n");
		TracePrintf(1, "cache: dequeueInode - Trying to dequeue when queue is empty\n");
		return;
    }
    if(frontInode == rearInode) {
		frontInode = rearInode = NULL;
    }else {
		frontInode->next->prev = NULL;
		frontInode = frontInode->next;
		frontInode->prev = NULL;
    }
}

void popQueueInode(struct inodeInfo * inode) {
    if(inode->prev != NULL && inode->next != NULL) {
		inode->prev->next = inode->next;
		inode->next->prev = inode->prev;
		inode->next = NULL;
		inode->prev = NULL;
    } else if(inode->prev != NULL && inode->next == NULL) {
		rearInode = rearInode->prev;
		rearInode->next = NULL;
		inode->next = NULL;
		inode->prev = NULL;
    } else if(inode->prev == NULL && inode->next != NULL) {
		dequeueInode();
		inode->next = NULL;
		inode->prev = NULL;
    }
	TracePrintf(1, "cache: popQueueInode - Successfully removed inode %p from queue\n", inode);
}


struct inodeInfo* getLruInode(int inodeNum) {
    struct inodeWrap* result = getInode(inodeNum);
    if(result == NULL) {
		TracePrintf(1, "cache: getLruInode - Cannot find corresponding inode to inodeNum %d\n", inodeNum);
		return nullInodeInfo;
    } else {
		popQueueInode(result->infoInode);
		enqueueInode(result->infoInode);
		return result->infoInode;
    }
}

struct blockInfo* readDiskBlock(int blockNum) {
	TracePrintf(1, "cache: readDiskBlock - about to read from disk block %d\n", blockNum);
    struct blockInfo* result = getLRUBlock(blockNum);
    if(result->blockNumber == -1) {
		result = (struct blockInfo*)malloc(sizeof(struct blockInfo));
		ReadSector(blockNum, (void*)(result->data));
		// reset dirty bit
		result->dirty = 0;
		result->blockNumber = blockNum;
		setLruBlock(blockNum, result);
		return getLRUBlock(blockNum);
    }else {
		return result;
    }
}

struct inodeInfo* readDiskInode(int inodeNum) {
	TracePrintf(1, "cache: readDiskInode - about to read from disk from inode number %d\n", inodeNum);
    struct inodeInfo* res = getLruInode(inodeNum);
    if(res->inodeNum == -1) {
		int blockNum = getBlockFromInode(inodeNum);
		struct blockInfo* temp = getLRUBlock(blockNum);
		if(temp->blockNumber == -1) {
			temp = readDiskBlock(blockNum);
		}
		res = (struct inodeInfo*) malloc(sizeof(struct inodeInfo));
		res->inodeNum = inodeNum;
		res->dirty = 0;
		res->inodeVal = (struct inode*)malloc(sizeof(struct inode));
		memcpy((res->inodeVal), temp->data+(inodeNum - (blockNum-1) * (BLOCKSIZE/INODESIZE)) * INODESIZE, INODESIZE);
		setLruInode(inodeNum, res); 
    }
    return res;
}


void setLruInode(int inodeNum, struct inodeInfo* newLruInode) {
	TracePrintf(1, "cache: setLruInode - about to set a new inode as LRU in cache\n");
    if(getInode(inodeNum) == NULL) {
		// a key needs to be removed.
		if(currInodeCacheNum >= INODE_CACHESIZE) {
				TracePrintf(1, "cache: setLruInode - a key needs to be removed from hashtable\n");
				int delKey = frontInode->inodeNum;
				removeInodeHashtable(delKey);
				sync();
				if(frontInode->dirty == 1) {
					int blockNum_to_write = getBlockFromInode(delKey);
					int index = inodeNum - (BLOCKSIZE/INODESIZE) * (blockNum_to_write - 1);
					struct blockInfo *temp = readDiskBlock(blockNum_to_write);
					memcpy((void*)(temp->data + index * INODESIZE), (void*)(frontInode->inodeVal), INODESIZE);
					temp->dirty = 1;
				}
				// remove and decrement counter
				dequeueInode();
				removeInodeHashtable(delKey);
				currInodeCacheNum--;
			}
			// add and increment counter
			enqueueInode(newLruInode);
			putInodeHashtable(inodeNum, newLruInode);
			currInodeCacheNum++;
			return;
    }else {
		struct inodeWrap *cachedInode = getInode(inodeNum);
		popQueueInode(cachedInode->infoInode);
		enqueueInode(newLruInode);
		putInodeHashtable(inodeNum, newLruInode);
		return;
    }
}