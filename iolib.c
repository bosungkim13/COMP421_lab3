#include "yfs.h"
#include "cache.h"
#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <comp421/filesystem.h>
#include <comp421/iolib.h>

struct openFile {
    short inode;
    int position;
};

struct openFile files[MAX_OPEN_FILES];

int numOpenFiles = 0;
int isFilesInit = 0;
short currDir = ROOTINODE;

void initFiles() {
    TracePrintf(1, "iolib: initFiles - initalizing files array\n");
    int i;
    for(i = 0; i < MAX_OPEN_FILES; i++) {
        files[i].inode = -1;
        files[i].position = 0;
    }
    isFilesInit = 1;
}


int parsePathname(char* oldName, char* newName) {
    TracePrintf(3, "cache: parsePathname - about to parse %s\n", oldName);
    int len = strlen(oldName);
    int i = 0; 
    int j = 0;

    for(i = 0; i < len; i++) {
        //get rid of consecutive slashes
        if(oldName[i] == '/' && oldName[i+1] == '/') {
            continue;
        }
        newName[j] = oldName[i];
        j++;
    }
    int newlen = j;
    //change last character from slash to period if needed
    if(newName[newlen-1] == '/') {
        newName[newlen] = '.';
        newName[newlen+1] = '\0';
        newlen++;
    } else{
        newName[newlen] = '\0';
    }
    //return length of pathname
    return newlen;
}

int sendYFS(uint8_t code, void** args, int* argSizes, int numArgs) {
    char message[MESSAGE_SIZE];
    message[0] = (char)code;
    int i;
    int offset = 1;
    for(i = 0; i < numArgs; i++) {
        if(*(void**)args[i] == NULL && argSizes[i] == sizeof(void*)) {
            TracePrintf(1, "iolib: sendYFS - invalid argument... null pointer\n");
            return ERROR;
	    }
        memcpy(message + offset, (char**)(args[i]), argSizes[i]);
        offset += argSizes[i];
    }
    // result should contain the integer that is the message reply value
    int success = Send(message, - FILE_SYSTEM);
    int res = *(int*)message;
    if(success == -1 || res == -1) {
        switch(code) {
            case YFS_OPEN:
                printf("iolib: Yalnix File System failed to open file\n"); 
                break;
            case YFS_CLOSE:
                printf("iolib: Yalnix File System failed to close file\n"); 
                break;
            case YFS_CREATE:
                printf("iolib: Yalnix File System failed to create file\n"); 
                break;
            case YFS_READ:
                printf("iolib: Yalnix File System failed to read file\n"); 
                break;
            case YFS_WRITE:
                printf("iolib: Yalnix File System failed to write to file\n"); 
                break;
            case YFS_SEEK:
                printf("iolib: Yalnix File System failed to seek file\n"); 
                break;
            case YFS_LINK:
                printf("iolib: Yalnix File System failed to create link\n"); 
                break;
            case YFS_UNLINK:
                printf("iolib: Yalnix File System failed to unlink\n"); 
                break;
            case YFS_SYMLINK:
                printf("iolib: Yalnix File System failed to create symbolic link\n"); 
                break;
            case YFS_READLINK:
                printf("iolib: Yalnix File System failed to read link\n"); 
                break;
            case YFS_MKDIR:
                printf("iolib: Yalnix File System failed to make directory\n"); 
                break;
            case YFS_RMDIR:
                printf("iolib: Yalnix File System failed to remove directory\n"); 
                break;
            case YFS_CHDIR:
                printf("iolib: Yalnix File System failed to change directory\n"); 
                break;
            case YFS_STAT:
                printf("iolib: Yalnix File System failed to get stats\n"); 
                break;
            case YFS_SYNC:
                printf("iolib: Yalnix File System failed to sync storage\n"); 
                break;
            case YFS_SHUTDOWN:
                printf("iolib: Yalnix File System failed to shutdown\n"); 
                break;
        }
    }
    return res;
}

int Open(char* pathname) {
    TracePrintf(1, "iolib: Open - Opening %s\n", pathname);
    if (!isFilesInit){
        initFiles();
    }
    if (numOpenFiles >= MAX_OPEN_FILES) {
        printf("iolib: Maximum count of files already open!\n");
        return 0;
    }

    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    int argSizes[3] = {sizeof(pathname), sizeof(pathSize), sizeof(currDir)};
    void* args[3] = {(void*)&pathname, (void*)&pathSize, (void*)&currDir};
    int inode = sendYFS(YFS_OPEN, args, argSizes, 3);

    int i;
    // free resources
    free(pathname);
    // look for available inode
    for(i = 0; i < MAX_OPEN_FILES; i++) {
        if(files[i].inode == -1) {
            files[i].inode = inode;
            numOpenFiles++;
            return i;
        }
    }
    return ERROR;
}

int Close(int fd) {
    TracePrintf(1, "iolib: Close - Closing file descriptor %d\n", fd);
    if (!isFilesInit){
        initFiles();
    }
    if(fd >= MAX_OPEN_FILES|| fd < 0 || files[fd].inode == -1) {
        TracePrintf(1, "iolib: Close - File descriptor %d is invalid\n", fd);
        printf("iolib: Invalid file descriptor!\n");
        return ERROR;
    }
    // reset inode
    files[fd].inode = -1;
    files[fd].position = 0;
    numOpenFiles--;
    return 0;
}

int Create(char *pathname) {
    TracePrintf(1, "iolib: Create - Creating file with path %s\n", pathname);
    if(!isFilesInit){
        initFiles();
    }
    if(MAX_OPEN_FILES <= numOpenFiles) {
        TracePrintf(1, "iolib: Create - Maximum number of files already open\n");
        printf("iolib: Maximum number of files already open. Cannot open more files!\n");
        return 0;
    }

    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    int argSizes[3] = {sizeof(pathname), sizeof(pathSize), sizeof(currDir)};
    void* args[3] = {(void*)&pathname, (void*)&pathSize, (void*)&currDir};
    int inode = sendYFS(YFS_CREATE, args, argSizes, 3);

    // free resources
    free(pathname);

    int i;
    for(i = 0; i < MAX_OPEN_FILES; i++) {
        // check for first unoccupied inode
        if(files[i].inode == -1) {
            files[i].inode = inode;
            numOpenFiles++;
            TracePrintf(1, "iolib: Create - Succesfully created file with path %s\n", pathname);
            return i;
        }
    }
    return ERROR;
}

int Read(int fd, void *buf, int size) {
    TracePrintf(1, "iolib: Read - Creating from file descriptor %d\n", fd);
    if(!isFilesInit){
        initFiles();
    }
	
    if(fd >= MAX_OPEN_FILES || fd < 0 || files[fd].inode == -1) {
        TracePrintf(1, "iolib: Read - Invalid file descriptor\n");
        printf("iolib: Invalid file descriptor\n");
        return ERROR;
    }
    int position = files[fd].position;
    short inode = files[fd].inode;
    int argSizes[4] = {sizeof(buf), sizeof(size), sizeof(inode), sizeof(position)};
    void* args[4] = {(void*)&buf, (void*)&size, (void*)&inode, (void*)&position};
    int result = sendYFS(YFS_READ, args, argSizes, 4);
    if(result == -1){
        return ERROR;
    }
	// update file position
    files[fd].position += result;
    
    return result;
}

int Write(int fd, void *buf, int size) {
    TracePrintf(1, "iolib: Write - Writing to file descriptor %d\n", fd);
    if(!isFilesInit) {
        initFiles();
    }
	
    if(fd < 0 || fd >= MAX_OPEN_FILES || files[fd].inode == -1) {
        TracePrintf(1, "iolib: Write - Writing to file descriptor %d\n", fd);
        printf("iolib: Invalid file descriptor\n");
        return ERROR;
    }

    short inode = files[fd].inode;
    int position = files[fd].position;
    int argSizes[4] = {sizeof(buf), sizeof(size), sizeof(inode), sizeof(position)};
    void* args[4] = {(void*)&buf, (void*)&size, (void*)&inode, (void*)&position};
    int res = sendYFS(YFS_WRITE, args, argSizes, 4);

    if(res == -1){
        TracePrintf(1, "iolib: write - Failed to write to file descriptor %d\n", fd);
        return ERROR;
    }

    files[fd].position += res;
    TracePrintf(1, "iolib: Write - Finished writing to file descriptor %d\n", fd);
    return res;
}

int Seek(int fd, int offset, int whence) {
    TracePrintf(1, "iolib: Seek - Start seeking on file descriptor %d with offset %d and whence %d\n", fd, offset, whence);

    if(fd < 0 || fd >= MAX_OPEN_FILES || files[fd].inode == -1) {
        TracePrintf(1, "iolib: Seek - Invalid file descriptor\n");
        printf("iolib: Invalid file descriptor\n");
        return ERROR;
    }
    if(!isFilesInit){
        initFiles();
    }

    int updatedPosition;

    switch(whence) {
        case SEEK_SET:
            updatedPosition = offset;
            break;
        case SEEK_CUR:
            updatedPosition = files[fd].position + offset;
            break;
        case SEEK_END:
        ;
            void* args[1] = {(void*)&files[fd].inode};
            int argSizes[1] = {sizeof(files[fd].inode)};
            updatedPosition = sendYFS(YFS_SEEK, args, argSizes, 1) - offset;
            break;
        default:
            TracePrintf(1, "iolib: Seek - whence value of %d is invalid\n", whence);
            printf("iolib: whence value invalid\n");
    }

    if(updatedPosition < 0) {
        TracePrintf(1, "iolib: Seek - Updated file position would be negative with value of %d\n", updatedPosition);
        return ERROR;
    }

    files[fd].position = updatedPosition;
    TracePrintf(1, "iolib: Seek - Seek Completed\n");
    return 0;
}

int Link(char *oldName, char *newName) {
    TracePrintf(1, "iolib: Link - Attempting to link %s to %s\n", oldName, newName);
    char* tempStr1 = oldName;
    oldName = (char*)malloc(strlen(oldName) + 2);
    int oldSize = parsePathname(tempStr1, oldName);
    char* tempStr2 = newName;
    newName = (char*)malloc(strlen(newName) + 2);
    int newSize = parsePathname(tempStr2, newName);

    int argSizes[5] = {sizeof(oldName), sizeof(oldSize), sizeof(newName), sizeof(newSize), sizeof(currDir)}; 
    void* args[5] = {(void*)&oldName, (void*)&oldSize, (void*)&newName, (void*)&newSize, (void*)&currDir};
    int res = sendYFS(YFS_LINK, args, argSizes, 5);
    // free resources
    free(oldName);
    free(newName);
    if(res == ERROR){
        TracePrintf(1, "iolib: Link - Send to yfs failed\n");
        return ERROR;
    }
    TracePrintf(1, "iolib: Link - Sucessfully completed link\n");
    return res;
}

int Unlink(char *pathname) {
    TracePrintf(1, "iolib: Unlink - Attempting to unlink %s\n", pathname);
    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    int argSizes[3] = {sizeof(pathname), sizeof(pathSize), sizeof(currDir)};
    void* args[3] = {(void*)&pathname, (void*)&pathSize, (void*)&currDir};
    int res = sendYFS(YFS_UNLINK, args, argSizes, 3);
    // free resources
    free(pathname);
    if(res == ERROR){
        TracePrintf(1, "iolib: Unlink - Send to yfs failed\n");
        return ERROR;
    }
    TracePrintf(1, "iolib: Unlink - Done with unlink\n");
    return res;
}
	
int SymLink(char *oldName, char *newName) {
    TracePrintf(1, "iolib: SymLink - Attempting to symlink %s to %s\n", oldName, newName);
    char* tempStr1 = oldName;
    oldName = (char*)malloc(strlen(oldName) + 2);
    int oldSize = parsePathname(tempStr1, oldName);
    char* tempStr2 = newName;
    newName = (char*)malloc(strlen(newName) + 2);
    int newSize = parsePathname(tempStr2, newName);
    int argSizes[5] = {sizeof(oldName), sizeof(oldSize), sizeof(newName), sizeof(newSize), sizeof(currDir)};
    void* args[5] = {(void*)&oldName, (void*)&oldSize, (void*)&newName, (void*)&newSize, (void*)&currDir};
    int res = sendYFS(YFS_SYMLINK, args, argSizes, 5);

    // free resources
    free(oldName);
    free(newName);
    if(res == ERROR){
        TracePrintf(1, "iolib: SymLink - Send to yfs failed\n");
        return ERROR;
    }
	TracePrintf(1, "iolib: SymLink - symlink complete\n");
    return 0;
}

int ReadLink(char *pathname, char *buf, int len) {
    TracePrintf(1, "iolib: ReadLink - Started with %s\n", pathname);
    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    int argSizes[5] = {sizeof(pathname), sizeof(pathSize), sizeof(buf), sizeof(len), sizeof(currDir)};
    void* args[5] = {(void*)&pathname, (void*)&pathSize, (void*)&buf, (void*)&len, (void*)&currDir};
    int res = sendYFS(YFS_READLINK, args, argSizes, 5);

    // free resources
    free(pathname);
    if(res == ERROR){
        TracePrintf(1, "iolib: ReadLink - Send to yfs failed\n");
        return ERROR;
    }
    TracePrintf(1, "iolib: ReadLink - readlink complete\n");
    return res;
}

int MkDir(char *pathname) {
    TracePrintf(1, "iolib: MkDir - Making directory %s\n", pathname);
    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    int argSizes[3] = {sizeof(pathname), sizeof(pathSize), sizeof(currDir)};
    void* args[3] = {(void*)&pathname, (void*)&pathSize, (void*)&currDir};
    int res = sendYFS(YFS_MKDIR, args, argSizes, 3);
    free(pathname);
    if(res == ERROR){
        TracePrintf(1, "iolib: MkDir - Send to yfs failed\n");
        return ERROR;
    }
    TracePrintf(1, "iolib: MkDir - Sucessfully created directory\n");
    return res;
}

int RmDir(char *pathname) {
    TracePrintf(1, "iolib: RmDir - Attempting to remove %s\n", pathname);
    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    int argSizes[2] = {sizeof(pathname), sizeof(pathSize)};
    void* args[2] = {(void*)&pathname, (void*)&pathSize};
    int res = sendYFS(YFS_RMDIR, args, argSizes, 2);
    free(pathname);
    if(res == ERROR){
        TracePrintf(1, "iolib: RmDir - Send to yfs failed\n");
        return ERROR;
    }
    TracePrintf(1, "iolib: RmDir - Directory successfully removed\n");
    return res;
}

int ChDir(char *pathname) {
    TracePrintf(1, "iolib: ChDir - Changing current directory to %s\n", pathname);
    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    void* args[3] = {(void*)&pathname, (void*)&pathSize, (void*)&currDir};
    int argSizes[3] = {sizeof(pathname), sizeof(pathSize), sizeof(currDir)};
    short new_inum = sendYFS(YFS_CHDIR, args, argSizes, 3);
    free(pathname);
    
    if(new_inum == ERROR) {
        TracePrintf(1, "iolib: ChDir - Couldn't retrieve directory\n");
	    return ERROR;
    }
    currDir = new_inum;
    TracePrintf(1, "iolib: ChDir - Successfully changed directory\n");
    return 0;
}

int Stat(char *pathname, struct Stat* statbuf) {
    TracePrintf(1, "iolib: Stat - Starting to retrieve stats for %s\n", pathname);
    char* tempStr = pathname;
    pathname = (char*)malloc(strlen(pathname) + 2);
    int pathSize = parsePathname(tempStr, pathname);
    void* args[4] = {(void*)&pathname, (void*)&pathSize, (void*)&statbuf, (void*)&currDir};
    int argSizes[4] = {sizeof(pathname), sizeof(pathSize), sizeof(statbuf), sizeof(currDir)};
    int res = sendYFS(YFS_STAT, args, argSizes, 4);

    // free resources
    free(pathname);
    if(res == ERROR){
        TracePrintf(1, "iolib: Stat - Send to yfs failed\n");
        return ERROR;
    }
    TracePrintf(1, "iolib: Stat - Sucessfully retrieved stats\n");
    return res;
}

int Sync(void) {
    return sendYFS(YFS_SYNC, NULL, NULL, 0);
}

int Shutdown(void) {
    return sendYFS(YFS_SHUTDOWN, NULL, NULL, 0);
}