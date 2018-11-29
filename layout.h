#define _BSD_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <libgen.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>

#define BLOCKSIZE 4096
#define BLOCK_NO 95		//change here 100-95
#define INODE_SIZE 80
#define SECTOR_SIZE 84
#define INODE_NO 45
#define SUB_NO 59
#define MAX_NAME_LEN 64
#define SECT_SIZE 256
#define METADATA 20480
//#define MAX_MAP 3
#define AT (1 << 0)
#define CT (1 << 1)
#define MT (1 << 2)

typedef enum {
	ORDINARY,
	DIRECTORY,
	FREE
} file_type;

//Stores fs info
void *fs;

//Superblock
struct superblock {
int magic;  //sequence of bytes to identify format of file
int blocksize; //size of a block(4096)
int  inode_table_size; //total no. of blocks for inode
int  inode_count;  //no. of inodes per block
int  data_block_count; //total no. of data blocks
int root_dir; //root's inode number
int max_file_length;//max file size=4096x2=8192 bytes
};

struct superblock *super;



struct myinode {
    ino_t st_id;            //inode id
    mode_t st_mode;         //permissions
    nlink_t st_nlink;       //number of hard links
    off_t st_size;          //size
    //blkcnt_t st_blocks;     //number of blocks referred to
		
	uid_t st_uid;           //user id
    gid_t st_gid;           //group id
  int sector_pointer;   //which is the starting sector
	int sector_tuples;    //how many sector tuples =
    time_t st_mtim;         //modify time
    time_t st_atim;         //access time
    time_t st_ctim;         //creation time

    //int direct_blk[MAX_MAP];      //blocks mapped to
    file_type type;  //0-free    1-file      2-directory

};

struct myinode *root;

struct sector{
	int tuples[20];		//(start sector id,length)
	int extratuples;
};

//Reducing limit to 60 files in a directory and 64 characters in the name
struct mydirent {
    char name[MAX_NAME_LEN];      //name of dir
    char subs[SUB_NO][MAX_NAME_LEN];  //names of contents
    int sub_id[SUB_NO];     //inode ids of contents
    char extra[20];
};

struct filehandle {
  struct myinode *node;
  int o_flags;
};


void openfile();
int make_superblock();
int make_rootnode(ino_t st_id, file_type type, int blk, mode_t mode);
int make_rootdir(char* name);
int init_fs();
