#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <comp421/filesystem.h>
#include <comp421/iolib.h>

#define FILE_SYSTEM 1

#define MESSAGE_SIZE 32

#define YFS_OPEN 1
#define YFS_CLOSE 2
#define YFS_CREATE 3
#define YFS_READ 4
#define YFS_WRITE 5
#define YFS_SEEK 6
#define YFS_LINK 7
#define YFS_UNLINK 8
#define YFS_SYMLINK 9
#define YFS_READLINK 10
#define YFS_MKDIR 11
#define YFS_RMDIR 12
#define YFS_CHDIR 13
#define YFS_STAT 14
#define YFS_SYNC 15
#define YFS_SHUTDOWN 16

#define FREE 0
#define OCCUPIED 1

void initFreeLists();
int _ysfOpen(char *pathname, short currDir);
int _ysfCreate(char *pathname, short currDir);
int _ysfRead(void *buf, int size, short inode, int position);
int _ysfWrite(void *buf, int size, short inode, int position);
int _ysfSeek(short inode);
int _ysfLink(char *oldName, char *newName, short currDir);
int _ysfUnlink(char *pathname, short currDir);
int _ysfSymLink(char *oldName, char *newName, short currDir);
int _ysfReadLink(char *pathname, char *buf, int len, short currDir);
int _ysfMkDir(char *pathname, short currDir);
int _ysfRmDir(char *pathname, short currDir);
int _ysfChDir(char *pathname, short currDir);
int _ysfStat(char *pathname, struct Stat* statbuf, short currDir);
int _ysfSync(void);
int _ysfShutdown(void);
int processMessage(char* message, int pid);
