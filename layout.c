#include "layout.h"
#include <sys/time.h>


/*chmod is the command and system call which may change the access permissions to file system objects (files and directories). It may also alter special mode flags. The request is filtered by the umask. The name is an abbreviation of change mode
1 – can execute
2 – can write
4 – can read
3 (1+2) – can execute and write
6 (2+4) – can write and read

Position of the digit in value:
1 – what owner can
2 – what users in the file group(class) can
3 – what users not in the file group(class) can


MAP_SHARED
              Share this mapping.  Updates to the mapping are visible to
              other processes mapping the same region, and (in the case of
              file-backed mappings) are carried through to the underlying
              file.  (To precisely control when updates are carried through
              to the underlying file requires the use of msync(2).)
*/

void openfile() {
    const char *filepath = "/home/prajwal/Desktop/a";

    int fd;

    size_t fssize = BLOCK_NO*BLOCKSIZE;

    if((fd = open(filepath, O_RDWR, (mode_t)0600))==-1){
        fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
        if (fd == -1) {
            perror("Error opening file for writing");
            exit(EXIT_FAILURE);
        }

        // Stretch the file size to the size of the (mmapped) array of char
        int result = lseek(fd, fssize-1, SEEK_SET);
        if (result == -1) {
	        close(fd);
	        perror("Error calling lseek() to 'stretch' the file");
	        exit(EXIT_FAILURE);
        }

        result = write(fd, "", 1);
        if (result != 1) {
	        close(fd);
	        perror("Error writing last byte of the file");
	        exit(EXIT_FAILURE);
        }

        fs = mmap(0, fssize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (fs == MAP_FAILED) {
	        close(fd);
	        perror("Error mmapping the file");
	        exit(EXIT_FAILURE);
        }
		    printf("init_fs called\n");
        init_fs();

        return;

    }


	printf("openfile called\n");
    fs = mmap(0, fssize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fs == MAP_FAILED) {
	    close(fd);
	    perror("Error mmapping the file");
	    exit(EXIT_FAILURE);
    }

	memcpy(super, fs, sizeof(struct superblock));
    memcpy(root, fs+BLOCKSIZE, INODE_SIZE);  //doubt

    printf("FS Synced");
}

int make_superblock(){
	super->magic=29;
	super->blocksize=4096;
	super->inode_table_size=1;
	super->inode_count=45;
	super->data_block_count=90;
	super->root_dir=0;
	super->max_file_length=8192;
	memcpy(fs, super, sizeof(struct superblock));
	return 1;
}



int make_rootnode(ino_t st_id, file_type type, int blk, mode_t mode){
    printf("make_rootnode\n");
    root->st_mode=mode;
    root->st_nlink=2;
    root->st_id=st_id;
    root->type=type;
    root->st_size=BLOCKSIZE;
	root->sector_tuples=1;
	root->sector_pointer=BLOCKSIZE*3;
	root->st_uid = getuid();
    root->st_gid = getgid();
    memcpy(fs+BLOCKSIZE, root, INODE_SIZE);	//inode 0-root
    return 1;
}

int make_rootdir(char* name) {
    printf("make_rootdir\n");
    struct mydirent *dir = (struct mydirent *)malloc(sizeof(struct mydirent));
    memset(dir, 0, BLOCKSIZE);
    strcpy(dir->name, name);
    strcpy(dir->subs[0],".");
    dir->sub_id[0]=root->st_id;
    strcpy(dir->subs[1],"..");
    dir->sub_id[1]=root->st_id;
    for(int i=2; i<SUB_NO; i++)
        dir->sub_id[i]=-1;
    memcpy(fs+5*BLOCKSIZE, dir, BLOCKSIZE);
    free(dir);
    return 1;
}

int init_fs() {
    // printf("Made it here");
	  make_superblock();
    make_rootnode(0, DIRECTORY, 5, S_IFDIR | 0755); //changehere 2-5
    make_rootdir("/");

    printf("Initialised Root\n");

    struct myinode *ino = (struct myinode *)malloc(INODE_SIZE);
	struct sector *sec = (struct sector *)malloc(SECTOR_SIZE);
    memset(ino, 0, INODE_SIZE);
	memset(sec, 0, SECTOR_SIZE);
	ino->type=FREE;
    ino->st_size=0;
    ino->st_mode=0755;
	ino->sector_tuples=0;
	ino->sector_pointer=3*BLOCKSIZE;
	sec->extratuples=0;
	for(int j=0;j<20;j++)
		sec->tuples[j]=-1;

    for(int i=1; i<INODE_NO; i++) {
        ino->st_id=i;
		ino->sector_pointer+=SECTOR_SIZE;
    memcpy(fs+BLOCKSIZE+(i*INODE_SIZE), ino, INODE_SIZE);
		memcpy(fs+3*BLOCKSIZE+(i*SECTOR_SIZE), sec, SECTOR_SIZE);		//change here
    }
	//for root
	sec->tuples[0]=0;
	sec->tuples[1]=16;
	memcpy(fs+3*BLOCKSIZE, sec, SECTOR_SIZE);
    printf("Initialised rest of the inodes and sectortuples\n");


    char occupied[]="1111111111111111";

    memcpy(fs+2*BLOCKSIZE, occupied, 16*sizeof(char)); //sec_free by superblock

    memcpy(fs+2*BLOCKSIZE+16*sizeof(char), occupied, 16*sizeof(char)); //occupied by inode list-no. of free sectors is 0

	memcpy(fs+2*BLOCKSIZE+16*2*sizeof(char), occupied, 16*sizeof(char)); //occupied by free block list

	memcpy(fs+2*BLOCKSIZE+16*3*sizeof(char), occupied, 16*sizeof(char)); //occupied by sector tuples

	memcpy(fs+2*BLOCKSIZE+16*4*sizeof(char), occupied, 16*sizeof(char)); //occupied by extra sector tuples

	memcpy(fs+2*BLOCKSIZE+16*5*sizeof(char), occupied, 16*sizeof(char)); //occupied by root

	char unoccupied='0';
	for(int i=96;i<=1440;i++){
		memcpy(fs+2*BLOCKSIZE+i*sizeof(char), &unoccupied, sizeof(char));
	}

    free(ino);
	free(sec);
    printf("Initialised free block list\n");
    return 1;
}

/*
int main(){
	printf("Size of inode: %lu\n",sizeof(struct myinode));
	printf("Size of sector: %lu\n",sizeof(struct sector));
	fs=malloc(BLOCK_NO*BLOCKSIZE);
	root=(struct myinode*)malloc(sizeof(struct myinode));
	super=(struct superblock*)malloc(sizeof(struct superblock));
	memset(fs, 0, BLOCK_NO*BLOCKSIZE);
	//init_fs();
	//memset(root, 0, INODE_SIZE);
	//memset(super, 0, sizeof(struct superblock));
	openfile();
	printf("openfile worked\n");
}*/
