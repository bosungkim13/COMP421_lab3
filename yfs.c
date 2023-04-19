#include "yfs.h"
#include "cache.h"

struct dir_entry createEntry(short inum, char* filename) {
    TracePrintf(1, "yfs: createEntry - Creating %s\n", filename);
    struct dir_entry entry;
    entry.inum = inum;

    memset(&entry.name, '\0', DIRNAMELEN);
    int filenameLen = strlen(filename);
    if(filenameLen > DIRNAMELEN){
		filenameLen = DIRNAMELEN;
	}
    memcpy(&entry.name, filename, filenameLen);
	TracePrintf(1, "yfs: createEntry - Successfully created entry\n");
    return entry;
}

// FINISHED TO CREATEEnTRY
int checkDir(int dirInum, char* filename) {
	TracePrintf(1, "yfs: checkDir - Checking %s\n", filename);
    struct inodeInfo* info = readDiskInode(dirInum);
    if(info->inodeNum == ERROR) {
		printf("yfs: Cannot read directory %s\n", filename);
		return ERROR;
    }

    struct dir_entry entry;
    int numEntries = info->inodeVal->size / sizeof(struct dir_entry);
    
    int i;
    for(i = 0; i < numEntries; i++) {
	if(_ysfRead((void*)&entry, sizeof(entry), dirInum, i * sizeof(struct dir_entry)) == ERROR) {
        //printf("ERROR: read i think\n");
	    return ERROR;
	}
    //printf("checking for directory %s", filename);
 	if(strncmp(filename, entry.name, DIRNAMELEN) == 0) {
		TracePrintf(1, "yfs: checkDir - Sucessfully found directory\n");
	    return entry.inum;
	}
	
    }
    //printf("we ball\n");
    return 0;
}

int delFileFromDir(int dirInum, int fileInum) {
	TracePrintf(1, "fys: delFileFromDir - Attempting to delete file with dirInum %d and fileInum %d\n", dirInum, fileInum);
    struct inodeInfo* info = readDiskInode(dirInum);
    if(info->inodeNum == ERROR) {
		printf("fys: Unable to delete file\n");
		return ERROR;
    }

    struct dir_entry entry;
    int numEntries = info->inodeVal->size / sizeof(struct dir_entry);
    int i;
    for(i = 2; i < numEntries; i++) {
		if(_ysfRead((void*)&entry, sizeof(entry), dirInum, i * sizeof(struct dir_entry)) == ERROR) {
			printf("fys: Unable to delete file\n");
			return ERROR;
		}
		if(fileInum == entry.inum) {
			char* nullChar = "\0";
			struct dir_entry emptyEntry = createEntry(0, nullChar);
			_ysfWrite((void*)&emptyEntry, sizeof(emptyEntry), dirInum, i * sizeof(struct dir_entry));
			TracePrintf(1, "fys: delFileFromDir - Successfully deleted file\n");
			return 0;
		}
    }
	printf("fys: Unable to delete file\n");
    return ERROR;
}
 
int pathname2InodeNum(char *pathname, int procInum) {
	TracePrintf(1, "fys: pathname2InodeNum - attempting to retreive InodeNum for %s\n", pathname);
    if(pathname == NULL ) {
		return procInum;
    }
	int symLinkCount = 0;
    int curInode;
    char inodeName[DIRNAMELEN + 1];
    memset(inodeName,'\0',DIRNAMELEN + 1);

    // check if absolute path or not
    if(pathname[0] == '/') {
		curInode = ROOTINODE;
    }
    else{
		curInode = procInum;
    }
    
    // remove all unnecessary slashes
    while(*pathname == '/') {
		pathname++;
    }

    while(strlen(pathname) != 0) {
		int pathLen = strlen(pathname);
		memset(inodeName,'\0',DIRNAMELEN + 1);
		int i = 0;
		// don't consider slashes
		while(pathLen > 0 && *pathname == '/') {
			pathname++;
			pathLen--;
		}
		// look at characters that aren't slashes
		while(pathLen > 0 && *pathname != '/') {
			inodeName[i] = *pathname;
			pathname++;
			pathLen--;
			i++;
		}
		struct inodeInfo* info;
		int subInum = checkDir(curInode, inodeName);
		if(subInum <= 0) {
			TracePrintf(1, "fys: pathname2InodeNum - unable to parse pathname\n");
			printf("yfs: Unable to parse pathname\n");
			return ERROR;
		}
		info = readDiskInode(subInum);
		// symbolic link
		if(info->inodeVal->type == INODE_SYMLINK) {
			if(symLinkCount >= MAXSYMLINKS) {
				TracePrintf(1, "fys: pathname2InodeNum - Too many symlinks\n");
				printf("yfs: Too many symlinks\n");
				return ERROR;
			} else{
				int dataSize = info->inodeVal->size;
				char* newPathname = (char*)calloc(sizeof(char) * dataSize + pathLen + 1, 1);
				struct blockInfo* block = readDiskBlock(info->inodeVal->direct[0]);
				memcpy(newPathname, block->data, dataSize);

				// add the remaining unprocessed string
				if(pathLen > 0) {
					strcat(newPathname, "/");
					strcat(newPathname, pathname);
				}
				pathname = newPathname;
				symLinkCount++;
				
				if(newPathname[0] == '/') {
					// reset current node to root if symlink
					curInode = ROOTINODE;
					continue;
				}
			}
		}
		curInode = subInum;
    }
	TracePrintf(1, "fys: pathname2InodeNum - Successfully converted pathname to inode number\n");
    return curInode;
}

int getFreeBlock() {
	TracePrintf(1, "fys: getFreeBlock - Retrieving free block\n");
    int i ;
    for (i = 0; i < NUM_BLOCKS; ++i) {
		if(freeBlockList[i] == FREE) {
			freeBlockList[i] = OCCUPIED;
			return i;
		}
    }
    sync();
    initFreeLists();

	// get blocks from deleted files
    for (i = 0; i < NUM_BLOCKS; ++i) {
		if(freeBlockList[i] == FREE) {
			TracePrintf(1, "fys: getFreeBlock - Found a free block\n");
			return i;
		}
    }
	TracePrintf(1, "fys: getFreeBlock - No memory; cannot find a free block\n");
    printf("iolib: yfs out of memory\n");
    return ERROR;
}

// expand fileInode to hold "newSize" data
int expandFile(struct inodeInfo* info, int newSize) {
	TracePrintf(1, "fys: expandFile - Expanding file to %d\n", newSize);
    struct inode* fileInode = info->inodeVal;
    info->dirty = 1;
    if(newSize < fileInode->size) {
		return 0;
    }

    info->dirty = 1;
    // round up to the next blocksize
    int current = ((fileInode->size + (BLOCKSIZE-1)) / BLOCKSIZE) * BLOCKSIZE;
    
    if(current < BLOCKSIZE * NUM_DIRECT) {
		// direct blocks
        while(current < BLOCKSIZE * NUM_DIRECT && current < newSize) {
			int freeBlock = getFreeBlock();
			if(freeBlock == ERROR) {
				return ERROR;
			}
			struct blockInfo* info = readDiskBlock(freeBlock);
			info->dirty = 1;
			memset(info->data, '\0', BLOCKSIZE);

			fileInode->direct[current / BLOCKSIZE] = freeBlock;
			current += BLOCKSIZE;
        }
    }
    
    // If this is the first time growing into indirect size then allocate indirect block
    if(current < newSize && current == BLOCKSIZE * NUM_DIRECT) {
        int newIndirect = getFreeBlock();
		if(newIndirect == ERROR){
			return ERROR;
		}
		fileInode->indirect = newIndirect;
    }

    // if direct blocks not enough, then access indirect blocks
    if(current < newSize) {
		int extraBlockNum = fileInode->indirect;
		struct blockInfo * indirectBlock = readDiskBlock(extraBlockNum);
		indirectBlock->dirty = 1;
		int* int_array = (int*)(indirectBlock->data);
		while(current < BLOCKSIZE * (NUM_DIRECT + BLOCKSIZE / sizeof(int)) && current < newSize ) {
			int freeBlock = getFreeBlock();
			if(freeBlock == ERROR) {
				return ERROR;
			}
			int_array[current / BLOCKSIZE - NUM_DIRECT] = freeBlock;
			current += BLOCKSIZE;
		}
    }
    fileInode->size = newSize;
	TracePrintf(1, "fys: expandFile - Successfully expanded file\n");
    return 0;
}

char* getDataByPosition(struct inodeInfo* info, int position, int setDirty) {
	TracePrintf(1, "fys: getDataByPosition - Getting pointer to data in position at file\n");
    struct inode* fileInode = info->inodeVal;
    if(position > fileInode->size) {
		printf("yfs: Attempting to read past size of file\n");
		return NULL;
    }

    int fileBlockNum = position / BLOCKSIZE;

    if(fileBlockNum < NUM_DIRECT) {
		// within direct blocks
	struct blockInfo* infoDirect = readDiskBlock(fileInode->direct[fileBlockNum]);
	if(infoDirect->dirty != 1 && setDirty == 1)
	    infoDirect->dirty = setDirty;
        return infoDirect->data + position % BLOCKSIZE;
    }

    // if position is within indirect blocks
    struct blockInfo* ininfoDirect = readDiskBlock(fileInode->indirect);
    int targetNum = ((int*)(ininfoDirect->data))[fileBlockNum - NUM_DIRECT];

    struct blockInfo* targetInfo = readDiskBlock(targetNum);
    targetInfo->dirty = setDirty;
	TracePrintf(1, "fys: getDataByPosition - Successfully retrieved pointer to data in position at file\n");
    return ((char*)(targetInfo->data)) + position % BLOCKSIZE;
}

int addDirectory(short dirInum, struct dir_entry newEntry) {
	TracePrintf(1, "fys: addDirectory - Starting to add directory\n");
    struct inodeInfo* dirInfo = readDiskInode(dirInum);
    dirInfo->dirty = 1;

    if(dirInfo->inodeNum == -1 || dirInfo->inodeVal->type != INODE_DIRECTORY) {
		printf("ysf: invalid dir inode number\n");
    }

	struct dir_entry oldEntry;
    int dirSize = dirInfo->inodeVal->size;
    int position = 0;
    
    // search for a emptyEntry in directory first and overwrite with new entry if possible
    while(position < dirSize) {
		_ysfRead(&oldEntry, sizeof(oldEntry), dirInum, position);
		if(oldEntry.inum == 0) {
			int success = _ysfWrite(&newEntry, sizeof(newEntry), dirInum, position);
			if(success != ERROR) {
				struct inodeInfo* info = readDiskInode(newEntry.inum);
				info->inodeVal->nlink++;
				info->dirty = 1;
				return success;
			}
			else{
				return ERROR;
			}
		}
		position += sizeof(oldEntry);
    }
    
    // write new entry at the end of the file
    int success = _ysfWrite(&newEntry, sizeof(newEntry), dirInum, position);

    if(success == ERROR) {
		return ERROR;
    }
    else{
		struct inodeInfo* info = readDiskInode(newEntry.inum);
		info->inodeVal->nlink++;
		info->dirty = 1;
		return success;
    }
}

int getParentInum(char* pathname, short currDir) {
	TracePrintf(1, "fys: getParentInum - Retrieving parent inum of %s\n", pathname);
    // searching for last '/'
    int i;
    for(i = strlen(pathname) - 1; i >= 0; i--) {
		if(pathname[i] == '/') {
			break;
		}
    }
    int pathParentLen = i + 1;
    // inode of parent directory
    char* parentPath = (char*)malloc(pathParentLen + 1);
    memcpy(parentPath, pathname, pathParentLen);
    parentPath[pathParentLen] = '\0';

    if(!(pathParentLen == 1 && parentPath[0] == '/')) {
		parentPath[pathParentLen-1] = '\0';
    }

    short parentInum = pathname2InodeNum(parentPath, currDir);
    free(parentPath);

    if(parentInum == ERROR) {
		printf("ysf: cannot find path to parent directory\n");
		return ERROR;
    }
    return parentInum;
}


char* getFilename(char* pathname) {
	TracePrintf(1, "fys: getFilename - Getting filename\n");
    int i;
    for(i = strlen(pathname) - 1; i >= 0; i--) {
		if(pathname[i] == '/') {
			return pathname + i + 1;
		}
    }
	// returns a pointer to the filename of last file in the pathname
    return pathname;
}

// creates a new file under the given parent directory
int createFile(char* filename, short parentInum, int type) {
	TracePrintf(1, "fys: createFile - Creating file %s\n", filename);
    short fileInum = checkDir(parentInum, filename);
    if(fileInum == ERROR) {
		return ERROR;
    }
    else if(fileInum != 0) {
		printf("ysf: File already exists!!\n");
		return ERROR;
    }

    
    short i;
    for(i = 0; i < NUM_INODES; i++) {
		if(freeInodesList[i] == FREE) {
			fileInum = i;
			break;
		}
    }
    if(fileInum == NUM_INODES) {
		printf("ERROR: no more inodes left for new file\n");
		return ERROR;
    }
	// allocate inodes
    freeInodesList[i] = OCCUPIED;
    
    struct inodeInfo* fileInfo = readDiskInode(fileInum);
    struct inode* fileInode = fileInfo->inodeVal;
    fileInode->size = 0;
    fileInfo->dirty = 1;
    fileInode->type = type;
    fileInode->nlink = 0;
    fileInode->reuse++;

    // new directory entry
    struct dir_entry entry = createEntry(fileInum, filename);

    if(addDirectory(parentInum, entry) == ERROR) {
		// update accordingly if theres an error
		freeInodesList[fileInum] = FREE;
		fileInode->type = INODE_FREE;
		fileInode->nlink = 0;
		fileInode->reuse--;
		printf("ysf: Failed to add file to directory\n");
		return ERROR;
    }
    return fileInum;
}

void initFreeLists() {
    TracePrintf(1, "fys: initFreeLists - Initializing free lists\n");
    struct inodeInfo* i_info = readDiskInode(0);
    struct fs_header* header = (struct fs_header*)(i_info->inodeVal);

    NUM_INODES = header->num_inodes;
    NUM_BLOCKS = header->num_blocks;

    freeInodesList = (short*)malloc(NUM_INODES * sizeof(short));
    freeBlockList = (short*)malloc(NUM_BLOCKS * sizeof(short));
     
    int i;
    for (i = 0; i < NUM_BLOCKS; ++i) {
		freeBlockList[i] = FREE;
    }
    for (i = 0; i < NUM_INODES; ++i) {
		freeInodesList[i] = FREE;
    }
	// fs_header, root, and boot blocks are occupied
    freeInodesList[0] = OCCUPIED; 
    freeInodesList[1] = OCCUPIED;  
    freeBlockList[0] = OCCUPIED; 
    // inode blocks are OCCUPIED
    for(i = 1; i < 1 + ((NUM_INODES + 1) * INODESIZE) / BLOCKSIZE; i++) {
		freeBlockList[i] = OCCUPIED;
    }

    for(i = 1; i < NUM_INODES + 1; i++) {
		struct inode* currInode = readDiskInode(i)->inodeVal;
		
		if(currInode->type != INODE_FREE) {
			freeInodesList[i] = OCCUPIED;
			int j = 0;
			while(j < NUM_DIRECT && j * BLOCKSIZE < currInode->size) {
				freeBlockList[currInode->direct[j]] = OCCUPIED;
				j++;		    
			}

			// if file still has more blocks, explore indirect block as well
			if(j * BLOCKSIZE < currInode->size) {
				int* blockIndirect = (int*)(readDiskBlock(currInode->indirect)->data);
				int finalBlock = (currInode->size + (BLOCKSIZE-1)) / BLOCKSIZE;
				freeBlockList[currInode->indirect] = OCCUPIED;
				while(j < finalBlock) {
					freeBlockList[blockIndirect[j - NUM_DIRECT]] = OCCUPIED;
					j++;
				}

			}
		}
    }
}

int _ysfOpen(char *pathname, short currDir) {
    return pathname2InodeNum(pathname, currDir);
}

int _ysfCreate(char *pathname, short currDir) {
    short parentInum = getParentInum(pathname, currDir);
    char* filename = getFilename(pathname);
	int res = createFile(filename, parentInum, INODE_REGULAR);
    return res;
}

int _ysfRead(void *buf, int size, short inode, int position) {
    struct inodeInfo* info = readDiskInode(inode);
    if(info->inodeNum == -1) {
		printf("yfs: Invalid inode\n");
		return ERROR;
    }
    int offset = 0;
	struct inode* fileInode = info->inodeVal;
    // While buf has space and we haven't reached EOF
    while(offset < size && position + offset < fileInode->size) {
        char* data = getDataByPosition(info, position + offset, 0);
		// readable size is min of space left in the block, buffer, and file
		int readSize = readSize = BLOCKSIZE - (position + offset) % BLOCKSIZE;
		if(size - offset < readSize)
			readSize = size - offset;
		if(fileInode->size - (position + offset) < readSize)
			readSize = fileInode->size - (position + offset);
		memcpy(buf + offset, data, readSize);
		offset += readSize;
    }
    return offset;
}

int _ysfWrite(void *buf, int size, short inum, int position) {
    struct inodeInfo* info = readDiskInode(inum);
    info->dirty = 1;
    if(info->inodeNum == -1) {
		printf("yfs: Invalid inode number\n");
		return ERROR;
    }
    // expand it first if file too small
    if(expandFile(info, position + size) == ERROR) {
		return ERROR;
    }
    int offset = 0;
    while(offset < size && position + offset < info->inodeVal->size) {
		char * data = getDataByPosition(info, position + offset, 1);

		// writeable size is min of blocksize and space left in the buf
		int writeSize = BLOCKSIZE - (position + offset) % BLOCKSIZE;
		if(size - offset < writeSize){
			writeSize = size - offset;
		}
		memcpy(data, buf + offset, writeSize);
		offset += writeSize;
    }
    return offset;
}

int _ysfSeek(short inode) {
    struct inodeInfo* info = readDiskInode(inode);
    if(info->inodeNum == -1) {
		printf("yfs: Invalid inode number\n");
		return ERROR;
    }
    return info->inodeVal->size;
}

int _ysfLink(char *oldName, char *newName, short currDir) {
    short prevInum = pathname2InodeNum(oldName, currDir);
    struct inodeInfo* old_info = readDiskInode(prevInum);
    if(old_info->inodeNum == -1 || old_info->inodeVal->type == INODE_DIRECTORY) {
		printf("yfs: Unable to create link to given file\n");
		return ERROR;
    }
    char* newFilename = getFilename(newName);
    short parentInum = getParentInum(newName, currDir);
    struct dir_entry entry = createEntry(prevInum, newFilename);
    addDirectory(parentInum, entry);
    old_info->inodeVal->nlink++;
    old_info->dirty = 1;
    return 0;
}

int _ysfUnlink(char *pathname, short currDir) {
    short inum = pathname2InodeNum(pathname, currDir);
    struct inodeInfo* info = readDiskInode(inum);
    if(info->inodeNum == -1 || info->inodeVal->type == INODE_DIRECTORY) {
		printf("yfs: Unable to read link\n");
		return ERROR;
    }
    short parentInum = getParentInum(pathname, currDir);
    delFileFromDir(parentInum, inum);
    info->inodeVal->nlink--;
    info->dirty = 1;
    return 0;
}

int _ysfSymLink(char *oldName, char *newName, short currDir) {
    short parentInum = getParentInum(newName, currDir);
    char* filename = getFilename(newName);
    short inum = createFile(filename, parentInum, INODE_SYMLINK);
    if(inum == ERROR){
		return ERROR;
	}
    int res = _ysfWrite((void*)oldName, strlen(oldName), inum, 0);
    return res;
}

int _ysfReadLink(char *pathname, char *buf, int len, short currDir) {
	char* filename = getFilename(pathname);
    short parentInum = getParentInum(pathname, currDir);
    short inum = checkDir(parentInum, filename);
    return _ysfRead(buf, len, inum, 0);
}

int _ysfMkDir(char *pathname, short currDir) {
	char* filename = getFilename(pathname);
    short parentInum = getParentInum(pathname, currDir);
    short inum = createFile(filename, parentInum, INODE_DIRECTORY);
    if(inum == ERROR){
		return ERROR;
	}
    char* periodStr = ".";
    char* doublePeriodStr = "..";
    struct dir_entry doublePeriod = createEntry(parentInum, doublePeriodStr);
    struct dir_entry period = createEntry(inum, periodStr);
    addDirectory(inum, period);
    addDirectory(inum, doublePeriod);
    return 0;
}

int _ysfRmDir(char *pathname, short currDir) {

    if(strcmp(pathname, "/") * strcmp(pathname, ".") * strcmp(pathname, "..") == 0) {
		printf("ysf: Cannot remove protected pathname\n");
		return ERROR;
    }
    short inum = pathname2InodeNum(pathname, currDir);
    struct inodeInfo* info = readDiskInode(inum);
    if(info->inodeNum == -1 || info->inodeVal->type != INODE_DIRECTORY) {
		printf("ysf: Invalid directory\n");
		return ERROR;
    }

    int i;
    for(i = 2; i < info->inodeVal->size / sizeof(struct dir_entry); i++) {
		// remove any leftover entries
		struct dir_entry entry;
		_ysfRead((void*)&entry, sizeof(entry), inum, i * sizeof(entry));
		if(entry.inum != 0) {
			printf("ERROR: cannot remove non-empty directory\n");
			return ERROR;
		}
    }
    short parentInum = getParentInum(pathname, currDir);    
    delFileFromDir(parentInum, inum);
	info->dirty = 1;
    info->inodeVal->type = INODE_FREE;  
    freeInodesList[inum] = FREE;  
    return 0;
}

int _ysfChDir(char *pathname, short currDir) {
    short inum = pathname2InodeNum(pathname, currDir);
    if(readDiskInode(inum)->inodeVal->type != INODE_DIRECTORY) {
		printf("ysf: Invalid directory\n");
		return ERROR;
    }
    return inum;
}

int _ysfStat(char *pathname, struct Stat* statbuf, short currDir) {
    short inum = pathname2InodeNum(pathname, currDir);
    struct inodeInfo* info = readDiskInode(inum);
    if(info->inodeNum == -1) {
		printf("ysf: Invalid pathname\n");
		return ERROR;
    }
    statbuf->inum = info->inodeNum;
    statbuf->type = info->inodeVal->type;
    statbuf->size = info->inodeVal->size;
    statbuf->nlink = info->inodeVal->nlink;
    return 0;
}

int _ysfSync(void) {
    return sync();
}

int _ysfShutdown(void) {
    sync(); 
    return 0;
}

int processMessage(char* message, int pid) {
#pragma GCC diagnostic push /*ignore unavoidable gcc warning caused by unconventional casting */
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    
    char* current = message;
    uint8_t code = (uint8_t)(message[0]);
    int res;
        
    switch(code) {
	case YFS_OPEN:{
	    char* upathname; 
		int pathnameSize; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&currDir, current += sizeof(pathnameSize), sizeof(currDir));
	    char* pathname = (char*)calloc(pathnameSize + 1, sizeof(char));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfOpen(pathname, currDir);
	    free(pathname);
	    break;
	}
	case YFS_CREATE:{
	    char* upathname; 
		int pathnameSize; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&currDir, current += sizeof(pathnameSize), sizeof(currDir));
	    char* pathname = (char*)calloc(pathnameSize + 1, sizeof(char));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfCreate(pathname, currDir);
	    free(pathname);
	    break;
	}
	case YFS_READ:{
	    char* ubuf; 
		int usize; 
		short uinode; 
		int uposition;
	    memcpy(&ubuf, current += sizeof(code), sizeof(ubuf));
	    memcpy(&usize, current += sizeof(ubuf), sizeof(usize));
	    memcpy(&uinode, current += sizeof(usize), sizeof(uinode));
	    memcpy(&uposition, current += sizeof(uinode), sizeof(uposition));
	    char* buf = calloc(usize + 1, sizeof(char));
	    res = _ysfRead(buf, usize, uinode, uposition);
	    CopyTo(pid, ubuf, buf, usize + 1);
	    free(buf);
	    break;
	}
	case YFS_WRITE:{
	    char* ubuf; 
		int usize; 
		short uinode; 
		int uposition;
	    memcpy(&ubuf, current += sizeof(code), sizeof(ubuf));
	    memcpy(&usize, current += sizeof(ubuf), sizeof(usize));
	    memcpy(&uinode, current += sizeof(usize), sizeof(uinode));
	    memcpy(&uposition, current += sizeof(uinode), sizeof(uposition));
	    char* buf = calloc(usize + 1, sizeof(char));
	    CopyFrom(pid, buf, ubuf, usize + 1);
	    res = _ysfWrite(buf, usize, uinode, uposition);
	    free(buf);
	    break;
	}
	case YFS_SEEK:{
	    short inode;	    
	    memcpy(&inode, current += sizeof(code), sizeof(inode));
	    res = _ysfSeek(inode);
	    break;
	}
	case YFS_LINK:{
	    char* uoldName; 
		int uoldName_size; 
		char* unewName; 
		int unewName_size;
	    short currDir;
	    memcpy(&uoldName, current += sizeof(code), sizeof(uoldName));
	    memcpy(&uoldName_size, current += sizeof(uoldName), sizeof(uoldName_size));
	    memcpy(&unewName, current += sizeof(unewName_size), sizeof(unewName));
	    memcpy(&unewName_size, current += sizeof(unewName), sizeof(unewName_size));
	    memcpy(&currDir, current += sizeof(unewName_size), sizeof(currDir));
		char* newName = calloc(unewName_size + 1, sizeof(char));
	    char* oldName = calloc(uoldName_size + 1, sizeof(char));
	    CopyFrom(pid, oldName, uoldName, uoldName_size + 1);
	    CopyFrom(pid, newName, unewName, unewName_size + 1);
	    res = _ysfLink(oldName, newName, currDir);
	    break;
	}
	case YFS_UNLINK:{
	    char* upathname; 
		int pathnameSize; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&currDir, current += sizeof(pathnameSize), sizeof(currDir));
	    char* pathname = (char*)calloc(pathnameSize + 1, sizeof(char));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfUnlink(pathname, currDir);
	    free(pathname);
	    break;
	}
	case YFS_SYMLINK:{
	    char* uoldName; 
		int uoldName_size; 
		char* unewName; 
		int unewName_size;
	    short currDir;
	    memcpy(&uoldName, current += sizeof(code), sizeof(uoldName));
	    memcpy(&uoldName_size, current += sizeof(uoldName), sizeof(uoldName_size));
	    memcpy(&unewName, current += sizeof(unewName_size), sizeof(unewName));
	    memcpy(&unewName_size, current += sizeof(unewName), sizeof(unewName_size));
	    memcpy(&currDir, current += sizeof(unewName_size), sizeof(currDir));
	    char* oldName = calloc(uoldName_size + 1, sizeof(char));
	    char* newName = calloc(unewName_size + 1, sizeof(char));
	    CopyFrom(pid, oldName, uoldName, uoldName_size + 1);
	    CopyFrom(pid, newName, unewName, unewName_size + 1);
	    res = _ysfSymLink(oldName, newName, currDir);
	    break;
	}
	case YFS_READLINK:{
	    char* upathname; 
		int pathnameSize; 
		char* ubuf; 
		int ulen; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&ubuf, current += sizeof(pathnameSize), sizeof(ubuf));
	    memcpy(&ulen, current += sizeof(ubuf), sizeof(ulen));
	    memcpy(&currDir, current += sizeof(ulen), sizeof(currDir));
		char* buf = calloc(ulen + 1, sizeof(char));
	    char* pathname = calloc(pathnameSize + 1, sizeof(char));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfReadLink(pathname, buf, ulen, currDir);
	    CopyTo(pid, ubuf, buf, ulen + 1);
		free(buf);
	    free(pathname);
	    break;
	}
	case YFS_MKDIR:{
	    char* upathname; 
		int pathnameSize; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&currDir, current += sizeof(pathnameSize), sizeof(currDir));
	    char* pathname = (char*)calloc(pathnameSize + 1, sizeof(char));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfMkDir(pathname, currDir);
	    free(pathname);
	    break;
	}
	case YFS_RMDIR:{
	    char* upathname; 
		int pathnameSize; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&currDir, current += sizeof(pathnameSize), sizeof(currDir));
	    char* pathname = (char*)calloc(pathnameSize + 1, sizeof(char));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfRmDir(pathname, currDir);
	    free(pathname);
	    break;
	}
	case YFS_CHDIR:{
	    char* upathname; 
		int pathnameSize; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&currDir, current += sizeof(pathnameSize), sizeof(currDir));
	    char* pathname = (char*)calloc(pathnameSize + 1, sizeof(char));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfChDir(pathname, currDir);
	    free(pathname);
	    break;
	}
	case YFS_STAT:{
	    char* upathname; 
		int pathnameSize; 
		struct Stat* statBuf; 
		short currDir;
	    memcpy(&upathname, current += sizeof(code), sizeof(upathname));
	    memcpy(&pathnameSize, current += sizeof(upathname), sizeof(pathnameSize));
	    memcpy(&statBuf, current += sizeof(pathnameSize), sizeof(statBuf));
	    memcpy(&currDir, current += sizeof(statBuf), sizeof(currDir));
	    char* pathname = (char*)calloc(pathnameSize + 1, sizeof(char));
	    struct Stat* statbuf = (struct Stat*)calloc(1, sizeof(struct Stat));
	    CopyFrom(pid, pathname, upathname, pathnameSize + 1);
	    res = _ysfStat(pathname, statbuf, currDir);
	    CopyTo(pid, statBuf, statbuf, sizeof(struct Stat));
		free(statbuf);
	    free(pathname);
	    break;
	}
	case YFS_SYNC:{
	    res = _ysfSync();
	    break;
	}
	case YFS_SHUTDOWN:{
	    res = _ysfShutdown();
	    if(res == 0) {
			int i;
			for(i = 0; i < MESSAGE_SIZE; i++) {
				message[i] = '\0';
			}
			memcpy(message, &res, sizeof(res));
			Reply(message, pid);      
			printf("\nShutdown request successful. Terminating Yalnix File System.\n");
			Exit(0);
	    }
	    break;
	}
	default:{
	    res = ERROR;
	}

    }
    return res;
#pragma GCC diagnostic pop /*pop -Wint-to-pointer-cast ignore warning*/
}

int main(int argc, char** argv) {
    
    initCache();
    initFreeLists();    
    Register(FILE_SERVER);
    printf("File System Initialized\n");
    int pid = Fork();
    if(pid == 0) {
		Exec(argv[1], argv + 1);
		printf("No init. Halting machine new\n");
		Halt();
    }
    
    // process messages
    while(1) {
		char message[MESSAGE_SIZE];
		int pid = Receive(message);
		if(pid == -1) {
			printf("Receive() returned error\n");
			Exit(0);
		}
		if(pid == 0) {
			TracePrintf(1, "Recieve() returned 0 to avoid deadlock\n");
			Exit(0);
		}
		int res = processMessage(message, pid);
		int i;
		for(i = 0; i < MESSAGE_SIZE; i++) {
			message[i] = '\0';
		}
		// copy in res and reply
		memcpy(message, &res, sizeof(res));
		Reply(message, pid);      
    }   
    return 0;
}
