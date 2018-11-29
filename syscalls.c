#define FUSE_USE_VERSION 31
#define _BSD_SOURCE

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <mcheck.h>
#include <math.h>
#include "layout.h"

//clarify how to make data zero.-unlink and truncate
//are you making changs to the inode after writing?
//4K file stat gives 0 blocks, du gave 0
// after appending something stat gives 8 blocks, du gave 4

int getnodebypath(const char *path, struct myinode *parent, struct myinode *child);

int dir_add(ino_t pi_id, ino_t ci_id, int blk, char *name);

int dir_add_alloc(struct myinode *parent, const char *name, struct myinode *child);

int dir_remove(struct myinode *parent, struct myinode *child, const char *name);

int dir_find(struct myinode *parent, const char *name, int namelen, struct myinode *child);

// Utility Functions
char * get_dirname(const char *msg);
char * get_basename(const char *msg);

int dir_find(struct myinode *parent, const char *name, int namelen, struct myinode *child) {
	int start;
	memcpy(&start,fs+parent->sector_pointer,sizeof(int));
	struct mydirent *pdir = (struct mydirent *) (fs+METADATA+start*SECT_SIZE); //get dirent of parent

	for(int i=2;i<59;i++) { //search all subs from 2 (0 and 1 are for . and ..)
		if(pdir->sub_id[i]!=-1) { //if it maps to a valid inode
			if(strlen(pdir->subs[i])==namelen) { //if the length of the sub_name matches namelen
				if(strncmp(pdir->subs[i], name, namelen)==0) { //if sub_name matches name
				  int idchild = pdir->sub_id[i]; //get inode_id at that index
				  memcpy(child, fs+BLOCKSIZE+idchild*INODE_SIZE, INODE_SIZE); //retrieve child's inode information from fs
				  return 1;
				}
			}
		}
	}

	errno = ENOENT;

	return 0;
}

int dir_remove(struct myinode *parent, struct myinode *child, const char *name) {// inode of / inode of hello hello
printf("Entered dir_remove\n");
 char empty[4097];
  memset(empty,'0',4096);
  empty[4096]='\0';



  int start;
  //struct mydirent *pdir = (struct mydirent *) (fs+parent->direct_blk[0]*BLOCKSIZE);
  printf("parent's sector pointer:%d\n",parent->sector_pointer);//13548
 //commenting this// struct mydirent *pdir = (struct mydirent *) (fs+METADATA+ (*(int*)(fs+parent->sector_pointer))*SECT_SIZE);
  memcpy(&start,fs+parent->sector_pointer,sizeof(int));
  struct mydirent *pdir = (struct mydirent *) (fs+METADATA+start*SECT_SIZE);
printf("pdir created\n");
  if(S_ISDIR(child->st_mode)) {
    //struct mydirent *cdir = (struct mydirent *) (fs+child->direct_blk[0]*BLOCKSIZE);
	struct mydirent *cdir = (struct mydirent *) (fs+METADATA+ (*(int*)(fs+child->sector_pointer))*SECT_SIZE);
    for(int i=2; i<SUB_NO; i++) { //check if directory is empty
      if(cdir->sub_id[i]!=-1) {
        errno = ENOTEMPTY;
        return 0;
      }
    }

  }
  printf("child's sector pointer:%d\n",child->sector_pointer);//12288
  //13548-12288/84= 15th

 // for(int i=0; i<child->st_blocks; i++) {
 //   memset(fs+child->direct_blk[i]*BLOCKSIZE, 0, BLOCKSIZE); //clear the child's directory entry/file contents
 //   memcpy(fs+BLOCKSIZE+child->direct_blk[i]*sizeof(int), &empty, sizeof(int));
 // }
 int *arr=(int*)calloc(1024,sizeof(int));

 printf("Going to make data 0\n");
  memcpy(&start,fs+child->sector_pointer,sizeof(int));
  printf("start:%d\n",start); //start:2054847098 -error-check sector_tuple
  memcpy(fs+METADATA+(start*SECT_SIZE),arr,BLOCKSIZE);
 //memcpy(fs+METADATA+ (*(int*)(fs+child->sector_pointer))*SECT_SIZE, empty, BLOCKSIZE);
 //memcpy(fs+METADATA+ (*(int*)(fs+child->sector_pointer))*SECT_SIZE,arr,BLOCKSIZE);
  printf("Going to make changes to databitmap\n");
  memcpy(fs+2*BLOCKSIZE+ 16*5+(*(int*)(fs+child->sector_pointer)), empty,16*sizeof(char));
  printf("Going to remove entry from directory\n");
  for(int i=2;i<SUB_NO;i++) { //reset child's name and inode id in the parent dirent
    if(strcmp(name, pdir->subs[i])==0) {
      memset(pdir->subs[i], 0, MAX_NAME_LEN);
      pdir->sub_id[i]=-1;
       printf("removed successfully\n");
      return 1;
    }
  }

  errno=ENOENT;


  return 0;
}

static void set_stat(struct myinode *node, struct stat *stbuf) {
	stbuf->st_mode   = node->st_mode;
	stbuf->st_nlink  = node->st_nlink;
	stbuf->st_size   = node->st_size;
	stbuf->st_uid    = node->st_uid;
	stbuf->st_gid    = node->st_gid;
	stbuf->st_mtime  = node->st_mtim;
	stbuf->st_atime  = node->st_atim;
	stbuf->st_ctime  = node->st_ctim;
	stbuf->st_blksize = 4096;
	int sectors=node->sector_tuples;
	struct sector *sec=(struct sector*)malloc(sizeof(struct sector));
	memcpy(sec,fs+node->sector_pointer,sizeof(struct sector));
	/*int i,start,offo,blocks=0;
	for(i=0;i<sectors*2;i+=2){
		offo=sec->tuples[i+1];
		blocks+=offo;
	}*/
	stbuf->st_blocks=node->st_size/512;

}

static void set_time(struct myinode *node, int param) {

  time_t now = time(0);
  if(param & AT) node->st_atim = now;
  if(param & CT) node->st_ctim = now;
  if(param & MT) node->st_mtim = now;

}

// Utility functions

//trailing / characters-not counted as part of path name
char * get_dirname(const char *msg) {
  char *buf = strdup(msg);
  char *dir = dirname(buf);
  char *res = strdup(dir);
  free(buf);
  return res;
}

char * get_basename(const char *msg) {
  char *buf = strdup(msg);
  char *nam = basename(buf);
  char *res = strdup(nam);
  free(buf);
  return res;
}



int getnodebypath(const char *path, struct myinode *parent, struct myinode *child) { //returns the inode info of the child
  if(!S_ISDIR(parent->st_mode)) {
    errno = ENOTDIR;
    return 0;
  }

  if(path[1] == '\0') {
    memcpy(child, parent, INODE_SIZE);//child =parent=root
    return 1;
  }

  // Extract name from path
  const char *name = path + 1;
  int len = 0;
  const char *end = name;
  while(*end != '\0' && *end != '/') {
    end++;
    len++;
  }

  // Search directory
  struct myinode *node = (struct myinode *)malloc(sizeof(struct myinode));
  if(!dir_find(parent, name, len, node)) {//retrieve inode info of child
    errno = ENOENT;
    free(node);
    return 0;
  }

  // free(name);

  if(*end == '\0') {
    // Last node in path
    memcpy(child, node, INODE_SIZE);
    free(node);
    return 1;
  } else {
    // Not the last node in path (or a trailing slash)
    return getnodebypath(end, node, child);
  }
}




int dir_add(ino_t pi_id, ino_t ci_id,int blk,char *name) {
	struct mydirent *dir = (struct mydirent *)malloc(sizeof(struct mydirent));
	strcpy(dir->name, name);
	//set self and parent inodes
	strncpy(dir->subs[0], ".", 1);
	dir->sub_id[0] = ci_id;

	strncpy(dir->subs[1], "..", 2);
	dir->sub_id[1] = pi_id;

	//sub_id of -1 means that the directory can map a file name and id at that index
	for(int i=2;i<SUB_NO; i++) {
		dir->sub_id[i] = -1;
	}

	memcpy(fs+blk*BLOCKSIZE, dir, BLOCKSIZE);
	free(dir);

	return 1;
}

int dir_add_alloc(struct myinode *parent, const char *name, struct myinode *child) { //inode of / hello inode of hello
  struct mydirent *pdir = (struct mydirent *)malloc(sizeof(struct mydirent));
  int start;
  memcpy(&start,fs+parent->sector_pointer,sizeof(int));
  memcpy(pdir, fs+METADATA+start*SECT_SIZE, BLOCKSIZE);


  if(strlen(name)>MAX_NAME_LEN) {
    errno = ENAMETOOLONG;
    free(pdir);
    return 0;
  }

  for(int i=2; i<SUB_NO; i++) { //search all subs from 2 (0 and 1 are for . and ..)
    printf("inode %d has inum :%d",i, pdir->sub_id[i]);
    if(pdir->sub_id[i]==-1) { //if the directory can hold more files/dirs
      strcpy(pdir->subs[i], name); //copy the name of the file/dir
      pdir->sub_id[i]=child->st_id; //map the name to inode of file/dir
      memcpy(fs+METADATA+start*SECT_SIZE, pdir, BLOCKSIZE);
      free(pdir);
      return 1;
    }
  }

  free(pdir);
  errno = EMLINK; //directory cannot hold more files/dirs

  return 0;

}



static int initstat(struct myinode *node, mode_t mode) {
	//struct stat *stbuf = (struct stat*)malloc(sizeof(struct stat));
	//memset(stbuf, 0, sizeof(struct stat));
	node->st_mode = mode;
	node->st_uid = getuid();
	node->st_gid = getgid();
	node->st_nlink = 0;
	set_time(node, AT | MT | CT);
	//free(stbuf);
	return 1;
}


static int sector_entry(int st_id){

	printf("entered sector_entry\n");
	struct myinode*node=(struct myinode *)malloc(sizeof(struct myinode));
	memcpy(node,fs+BLOCKSIZE+(st_id)*INODE_SIZE,INODE_SIZE);
	printf("%d %lu\n",st_id, node->st_id);
	struct sector *sec = (struct sector *)malloc(sizeof(struct sector));

	printf("node->sector_pointer:%d\n",node->sector_pointer);

//printf("printing contents of  sectors meta data before changing:\n");

 //for(int i=0;i<4096;i++)
   // 	printf("%d ",*(int *)(fs+BLOCKSIZE*3+(i*sizeof(int))));
	printf("Going to look for free sectors:\n");
	int one=1;
 	int *arr=(int*)calloc(1,256);//initialises allocated memory to 0
  for(int i=0;i<16;i++){

		if(memcmp(fs+METADATA+(i*SECT_SIZE), arr,SECT_SIZE)==0)//free sector found
		{
				node->sector_tuples+=1;
				memcpy(&sec->tuples[0],&i,sizeof(int));
				memcpy(&sec->tuples[1],&one,sizeof(int));

				int arr[18];
				memset(arr,-1,18*sizeof(int));
				memcpy(&node->sector_pointer+2*(sizeof(int)),arr,18*sizeof(int));
				//sec->tuples[0]=i;
				//sec->tuples[1]=1;//initially,when file is created, only 1 sector is given to it. Can be increased while writing.
				sec->extratuples=0;
				memcpy(fs+BLOCKSIZE+(node->st_id)*INODE_SIZE, node,INODE_SIZE);
				memcpy(fs+node->sector_pointer,sec,SECTOR_SIZE);
				for(int i=0;i<node->sector_tuples;i+=2){
					printf("SECTOR ENTRY SECTORS %d %d\n",sec->tuples[i],sec->tuples[i+1]);
				}

// printf("printing contents of  sectors meta data after changing:\n");
//
//  for(int i=0;i<4096;i++)
//    	printf("%d ",*(int *)(fs+BLOCKSIZE*3+(i*sizeof(int))));

//printf("printing contents of  sectors:\n");
//for(int j=sec->tuples[0];j<256;j++)
	//	printf("%d",*((int *)(fs+METADATA+j*sizeof(int))));
			return 0;
		}
	}
	free(node);
	free(sec);
	return -ENOMEM;
}

static int get_req_sects(int st_id,int req_blocks){
	printf("entered sector_entry\n");
	struct myinode* node=(struct myinode *)malloc(sizeof(struct myinode));
	memcpy(node,fs+BLOCKSIZE+(st_id)*INODE_SIZE,INODE_SIZE);
	printf("%d %lu\n",st_id, node->st_id);
	struct sector *sec = (struct sector *)malloc(sizeof(struct sector));
	memcpy(sec,fs+node->sector_pointer,sizeof(struct sector));

	int sector_tuples = node->sector_tuples;
	int last_blk = sec->tuples[sector_tuples*2-2] + sec->tuples[sector_tuples*2 - 1];
	//last_blk = last_blk + sec->tuples[sector_tuples*2 - 1];		
	int i = last_blk;
	int flag = 0;
	// printf("node->sector_pointer:%d\n",node->sector_pointer);
	// printf("Going to look for free sectors:\n");
	char a='0';
	char b='1';
	// printf("Going to print data bitmap:\n");
	// for(int k=0;k<1440;k++)
	// 	printf("%c ",*(char *)(fs+2*BLOCKSIZE+k));

	while(req_blocks>0){
		if(memcmp(fs+2*BLOCKSIZE+80+i,&a,1)==0){
			printf("FREE %d\n",i);
			memcpy(fs+2*BLOCKSIZE+80+i,&b,1);
			if(i==last_blk){
				sec->tuples[sector_tuples*2-1]++;
				last_blk = i+1;
			}
			else{
				if(sector_tuples<10){
					sector_tuples++;
					sec->tuples[sector_tuples*2-2] = i;
					sec->tuples[sector_tuples*2-1] = 1;
					last_blk = i+1;
				}
				else{
					return -1;
				}
			}
			req_blocks--;
		}
		i++;
	}

	node->sector_tuples = sector_tuples;
	memcpy(fs+node->sector_pointer,sec,sizeof(struct sector));
	memcpy(fs+BLOCKSIZE+(st_id)*INODE_SIZE,node,INODE_SIZE);
	return 1;
}
static int inode_entry(const char *path, mode_t mode) {
	printf("entered inode_entry\n");

  //const int full=1, empty=0;
  char empty[]="0000000000000000" , full[]="1111111111111111";
  char secempty='0', secfull='1';
  struct myinode *node = (struct myinode *)malloc(sizeof(struct myinode));

  if(getnodebypath(path, root, node)) {
    errno = EEXIST;
    free(node);
    return -errno;
  }

  printf("\nfs here : %d\n",*(int *)(fs+node->sector_pointer));

  //Find parent
  char *dirpath = get_dirname(path);

  struct myinode *parent = (struct myinode *)malloc(sizeof(struct myinode));
  if(!getnodebypath(dirpath, root, parent)) {
    free(dirpath);
    free(parent);
    free(node);
    return -errno;
  }
  free(dirpath);



  //Check for free inodes
  for(int i=1; i<INODE_NO; i++) {
    memcpy(node, (struct myinode *)(fs+BLOCKSIZE+i*INODE_SIZE), INODE_SIZE);
    if(node->type==FREE) {
      if(!initstat(node, mode)) {
        free(node);
        free(parent);
        return -errno;
      }

      // Add entry in parent directory
      if(!dir_add_alloc(parent, get_basename(path), node)) {
        free(node);
        free(parent);
        return -errno;
      }

      else {//need to allocate space
        if(S_ISDIR(node->st_mode)) {	//it's a directory
		  //Check for free blocks
		  int blk = 0;
		  int b;
		  for(b=6; b<BLOCK_NO; b++) {
			if(memcmp(fs+2*BLOCKSIZE+b*16*sizeof(char), empty, 16*sizeof(char))==0) {
			  blk = b;
			  break;
			}
		  }

		  if(blk == 0){
			errno = ENOMEM;
			free(node);
			free(parent);
			return -errno;
		  }
          if(dir_add(parent->st_id, node->st_id, blk, get_basename(path))) {
            node->st_size=BLOCKSIZE;
            node->st_nlink=2;
            node->type = DIRECTORY;
			node->sector_tuples=1;
			*(int*)(fs+node->sector_pointer)=(blk-5)*16;
			*(int*)(fs+node->sector_pointer+sizeof(int))=16;
			//memset(fs+node->sector_pointer,(blk-5)*16,sizeof(int));
			//memset(fs+node->sector_pointer+sizeof(int), 16, sizeof(int));
            parent->st_nlink++;
            memcpy(fs+2*BLOCKSIZE+16*blk*sizeof(char), full, 16*sizeof(char));
          }
          else {
            free(node);
            free(parent);
            return -errno;
          }
        }
        else {//creating file
          node->st_nlink = 1;
          node->type = ORDINARY;
		  //Check for free blocks
		  int SEC = 0;
		  int s;
		  for(s=96; s<1440; s++) {
			if(memcmp(fs+2*BLOCKSIZE+s*sizeof(char), &secempty, sizeof(char))==0) {

			  SEC = s-80;
			  
			  break;
			}
		  }
		  int one=1;
      	  memcpy(fs+2*BLOCKSIZE+(SEC+80), &secfull, sizeof(char));
		  node->sector_tuples=1;
		  memcpy(fs+node->sector_pointer, &SEC, sizeof(int));
		  memcpy(fs+node->sector_pointer+sizeof(int), &one, sizeof(int));
		  printf("fs+sector_pointer : %d\n", *(int *)(fs+node->sector_pointer));
		  printf("Going to print data bitmap:\n");
		  for(int k=0;k<1440;k++)
		  	printf("%c ",*(char *)(fs+2*BLOCKSIZE+k));

		  //printf("%d ",*(int *)(fs+2*BLOCKSIZE+k*sizeof(int)));
		  	for(int i=0;i<21;i++)
printf("\n%d ",*(int *)(fs+node->sector_pointer+i*sizeof(int)));
printf("\n");

        }
        //node->direct_blk[0]=blk;
        //node->st_blocks=1;
        set_time(node, AT|CT|MT);
        set_time(parent, AT|MT);
        memcpy(fs+BLOCKSIZE+node->st_id*INODE_SIZE, node, INODE_SIZE);
        memcpy(fs+BLOCKSIZE+parent->st_id*INODE_SIZE, parent, INODE_SIZE);
        free(node);
        free(parent);
        return 0;
      }
    }
  }

  free(node);
  free(parent);

  return -ENOMEM;

}





//------------------------SYSTEM CALLS----------------------------------------
static int fs_getattr(const char *path, struct stat *stbuf) {
  struct myinode *node = (struct myinode *)malloc(sizeof(struct myinode));
  memset(node, 0, INODE_SIZE);
  if(!getnodebypath(path, root, node)) {
    return -errno;
  }

  set_stat(node, stbuf);

  // read_inode(node->st_id);

  return 0;
}





static int fs_mkdir(const char *path, mode_t mode) {
	// struct myinode *node;
	int i;
	//for(i=0;i<4096;i++)
	//	printf("%c",*(char *)(fs+BLOCKSIZE*6+i));
	int res = inode_entry(path, S_IFDIR | mode);
	printf("mkdir called\n");

	//for(i=0;i<4096;i++)
	//	printf("%c",*(char *)(fs+BLOCKSIZE*6+i));
	printf("\n\nsector block:\n");
	for(i=0;i<945;i++)
		printf("%d",*(int *)(fs+BLOCKSIZE*3+i*sizeof(int)));
	if(res) return res;

	return 0;
}


static int fs_rmdir(const char *path) {
	char *parent_path, *name;
	struct myinode *parent = (struct myinode *)malloc(sizeof(struct myinode));
	struct myinode *child = (struct myinode *)malloc(sizeof(struct myinode));

	if(!getnodebypath(path, root, child)) {
		return -errno;
	}

	parent_path = get_dirname(path);

	if(!getnodebypath(parent_path, root, parent)) {
		free(parent_path);
		return -errno;
	}
	free(parent_path);

	name = get_basename(path);

	if(!dir_remove(parent, child, name)) {
		free(name);
		return -errno;
	}

	child->type = FREE;
	//child->st_blocks = 0;
	child->sector_tuples=0;
	*(int*)(fs+child->sector_pointer)=-1;
	*(int*)(fs+sizeof(int)+child->sector_pointer)=-1;
	child->st_size = 0;
	child->st_nlink = 0;
	parent->st_nlink--;

	memcpy(fs+BLOCKSIZE+child->st_id*INODE_SIZE, child, INODE_SIZE);
	memcpy(fs+BLOCKSIZE+parent->st_id*INODE_SIZE, parent, INODE_SIZE);
	printf("rmdir called\n");
	int i;
	//for(i=0;i<4096;i++)
	//	printf("%c",*(char *)(fs+BLOCKSIZE*6+i));
	printf("\n\nsector block:\n");
	for(i=0;i<945;i++)
		printf("%d",*(int *)(fs+BLOCKSIZE*3+i*sizeof(int)));

	free(name);
	free(parent);
	free(child);

	return 0;
}


static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	struct myinode *current = (struct myinode *)malloc(sizeof(struct myinode));

	if(!getnodebypath(path, root, current)) {
		return -errno;
	}

	struct stat *cst = (struct stat *)malloc(sizeof(struct stat));
	set_stat(current, cst);

	filler(buf, ".",  cst, 0);

	if(current == root) {
		filler(buf, "..", cst, 0);
	}
	else {
		char *parent_path = get_dirname(path);
		struct myinode *parent = (struct myinode *)malloc(sizeof(struct myinode)); ;
		getnodebypath(parent_path, root, parent);
		// read_inode(parent->st_id);
		struct stat *pst = (struct stat *)malloc(sizeof(struct stat));
		set_stat(parent, pst);
		free(parent_path);
		filler(buf, "..", pst, 0);
	}

	struct mydirent *entry = (struct mydirent *) (fs+METADATA+(*(int*)(fs+current->sector_pointer))*SECT_SIZE);
	for(int i=2; i<SUB_NO; i++) {
		if(entry->sub_id[i] != -1) {
			struct myinode *child = (struct myinode *)malloc(sizeof(struct myinode));
			memset(child, 0, INODE_SIZE);
			memcpy(child, fs+BLOCKSIZE+(entry->sub_id[i])*INODE_SIZE, INODE_SIZE);
			struct stat *stbuf = (struct stat *)malloc(sizeof(struct stat));
			memset(stbuf, 0, sizeof(struct stat));
			set_stat(child, stbuf);
			filler(buf, entry->subs[i], stbuf, 0);
			}
	}

	return 0;
}
static int fs_mknod(const char *path, mode_t mode, dev_t rdev) {
printf("fs_mknod called\n");
}




static int fs_open(const char *path, struct fuse_file_info *fi) {
  struct myinode *node = (struct myinode *)malloc(sizeof(struct myinode));
  // memset(node, 0, INODE_SIZE);
  printf("open_file called\n");

  if(!getnodebypath(path, root, node)) {
  printf("here");
    return -errno;
  }

  set_time(node, AT);

  //Store filehandle information in fuse
  struct filehandle *fh = malloc(sizeof(struct filehandle));
  fh->node    = node;
  fh->o_flags = fi->flags;
  fi->fh = (uint64_t) fh;
	printf("In Open size %zu\n",node->st_size);

  return 0;
}



static int fs_creat(const char *path, mode_t mode, struct fuse_file_info *fi) {
printf("fs_creat called\n");
int res = inode_entry(path, mode);
printf("res=%d",res);
 struct myinode *node = (struct myinode *)malloc(sizeof(struct myinode));
  memset(node, 0, INODE_SIZE);

  if(!getnodebypath(path, root, node)) {
  	
    return -EEXIST;
  }
  printf("inode id of created file:%lu\n",node->st_id);
 


  //Store filehandle information in fuse
  struct filehandle *fh = malloc(sizeof(struct filehandle));
  fh->node    = node;
  fh->o_flags = fi->flags;
  fi->fh = (uint64_t) fh;

  if(res) return res;//error
	printf("File created\n");
  return 0;
}

static int fs_unlink(const char *path) {
printf("fs_unlink called\n");
  char *parent_path, *name;
  struct myinode *parent = (struct myinode *)malloc(sizeof(struct myinode));
  struct myinode *child = (struct myinode *)malloc(sizeof(struct myinode));
  struct sector* sec=(struct sector *)malloc(sizeof(struct sector));

  if(!getnodebypath(path, root, child)) {//retrieving inode of child
    return -errno;
  }


  printf("Child inode id :%lu\n",child->st_id);

  printf("path:%s\n",path);

  parent_path = get_dirname(path);
  printf("Going to print sector block");

  
  memcpy(sec,fs+child->sector_pointer,sizeof(struct sector));

/*
int startt;
memcpy(&startt,fs+child->sector_pointer,sizeof(int));
 printf("Current sector pointer contents:%d\n",startt);
 for(int j=0;j<21;j++)
 printf("%d ",*((int *)(fs+(child->sector_pointer+j*sizeof(int)))));
 char *zero=(char *)calloc(256,sizeof(char)); 
 //memset(zero,'0',256);
 memset(zero,0,256);
 printf("printing array which I'm going to use for memcpy");
 printf("%s",zero);
 printf("\n");
 for(int i=0;i<child->sector_tuples*2;i+=2){
		int start = sec->tuples[i];
		int offs = sec->tuples[i+1];
		printf("%d %d START OFFS\n",start,offs);
		for(int k=0;k<251;k++)
			printf("%c",*(char *)(fs+METADATA+start*(SECT_SIZE)+k));
		printf("\n");
		 
		
		for(int j=0;j<offs;j++)
			
			memcpy(fs+METADATA+(start+j)*(SECT_SIZE),zero,SECT_SIZE);
		//memcpy(fs+METADATA+(start)*(SECT_SIZE),zero,offs*SECT_SIZE);
		printf("after making it 0\n");
		for(int k=0;k<256;k++)
			printf("%c",*(char *)(fs+METADATA+(start)*(SECT_SIZE)+k));
		printf("\n");
		}
		//going to make sector meta data -1
	
		
 
 //memcpy(&child->sector_pointer,); what should this be after deleting?
for(int j=0;j<child->sector_tuples*2;j++){
		sec->tuples[j]=-1;
		
}
 sec->extratuples=0;
 child->sector_tuples=0; 
 memcpy(fs+child->sector_pointer, sec, SECTOR_SIZE);
  printf("Current sector pointer contents after deleting\n");
 for(int j=0;j<21;j++)
 printf("%d ",*((int *)(fs+child->sector_pointer+j*sizeof(int))));


*/
  
   char *zero=(char *)calloc(256,sizeof(char)); 
 //memset(zero,'0',256);
 memset(zero,0,256);
 printf("printing array which I'm going to use for memcpy");
 printf("%s",zero);
 printf("\n");
 for(int i=0;i<child->sector_tuples*2;i+=2){
		int start = sec->tuples[i];
		int offs = sec->tuples[i+1];
		printf("%d %d START OFFS\n",start,offs);
		for(int k=0;k<256;k++)
			printf("%c",*(char *)(fs+METADATA+start*(SECT_SIZE)+k));
		printf("\n");
		 
		
		for(int j=0;j<offs;j++)
			
			memcpy(fs+METADATA+start*(SECT_SIZE),zero,SECT_SIZE);
		//memcpy(fs+METADATA+(start)*(SECT_SIZE),zero,offs*SECT_SIZE);
		printf("after making it 0\n");
		for(int k=0;k<256;k++)
			printf("%c",*(char *)(fs+METADATA+start*(SECT_SIZE)+k));
		printf("\n");
		}
  
  
  
  printf("parent_path: %s\n",parent_path);
  if(!getnodebypath(parent_path, root, parent)) {
    free(parent_path);
    return -errno;
  }
  free(parent_path);

  name = get_basename(path);
  printf("base_name: %s\n",name);
  if(!dir_remove(parent, child, name)) {
    free(name);
    return -errno;
  }

  child->type = FREE;
  child->st_size = 0;
  child->st_nlink = 0;
 child->sector_tuples=0;
 
 printf("child->sector_pointer: %d\n",child->sector_pointer);
 int start;
 memcpy(&start,fs+child->sector_pointer,sizeof(int));
 printf("Current sector pointer contents:%d\n",start);
 for(int j=0;j<21;j++)
 printf("%d ",*((int *)(fs+(child->sector_pointer+j*sizeof(int)))));
for(int j=0;j<20;j++){
		sec->tuples[j]=-1;
		
		}
 sec->extratuples=0;
 
 //memcpy(fs+3*BLOCKSIZE+start*SECTOR_SIZE, sec, SECTOR_SIZE);
  memcpy(fs+child->sector_pointer, sec, SECTOR_SIZE);
 //memcpy(&child->sector_pointer,); where should this point to
 //memcpy(sec,a,20*sizeof(int));
 
 printf("Contents after deleting:\n");
 for(int j=0;j<21;j++)
 printf("%d ",*((int *)(fs+child->sector_pointer+j*sizeof(int))));
 child->sector_tuples=0;
  parent->st_nlink--;

  memcpy(fs+BLOCKSIZE+parent->st_id*INODE_SIZE, parent, INODE_SIZE);
  memcpy(fs+BLOCKSIZE+child->st_id*INODE_SIZE, child, INODE_SIZE);

  free(name);

  return 0;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	//33345 Write Mode 33793 Append Mode 32769 Truncate Mode
	//Working Part
	printf("=====================================fs_write called offset is %zu\n",offset);
	printf("%s\n",buf);
	struct filehandle *fh = (struct filehandle *) fi->fh;
	printf("FLAGS %d\n",fh->o_flags);
	struct myinode * node = (struct myinode *) fh->node;
	struct sector * sector = (struct sector *)malloc(sizeof(struct sector));

	int sector_tuples = node->sector_tuples;
	int sector_ptr = node->sector_pointer;
	off_t size1 = node->st_size;
	memcpy(sector,fs+sector_ptr,sizeof(struct sector));

	int last_blk = sector->tuples[sector_tuples*2-2];
	int for_later = sector_tuples*2-2; int for_later2 = sector->tuples[sector_tuples*2-1];
	last_blk = last_blk + sector->tuples[sector_tuples*2-1] - 1;
	int last_blk_offs,last_blk_offs_ind;

	int how_much_write;
	memcpy(&last_blk_offs,fs+METADATA+(last_blk+1)*(SECT_SIZE)-4,sizeof(int));
	printf("INITIAL offset set to %d\n",last_blk_offs);

	how_much_write = SECT_SIZE - last_blk_offs - 5;
	int xs = (int)size;
	printf("LAST sector is %d\n",last_blk);
	set_time(node, CT | MT);
	printf("How much write %d , text to be written %d\n",how_much_write,xs);

	//Write Mode(if content to be written is less than one sector size)
	if(SECT_SIZE>=(int)xs && fh->o_flags==33345 || fh->o_flags==32769){
		printf("WRITING less\n stlen(buf) %d, last_blk=%d sizeparameter=%d\n",(int)xs,last_blk,(int)size);
		memcpy(fs+METADATA+last_blk*(SECT_SIZE),buf,(int)xs);		//why is it writing to the last sector?????
		last_blk_offs = last_blk_offs + xs;
		// fh->node->st_size += fh->node->st_size>strlen(buf)?0:strlen(buf);
		fh->node->st_size += xs;
		memcpy(fs+BLOCKSIZE+fh->node->st_id*INODE_SIZE,fh->node,INODE_SIZE);	//what is this????
		printf("HERE minsize %zu\n",fh->node->st_size);
		printf("HERE offset set to %d\n",last_blk_offs);
		memcpy(fs+METADATA+(last_blk+1)*(SECT_SIZE)-4,&last_blk_offs,sizeof(int));  //Update Offset Info

		// memcpy(fs+METADATA+(last_blk+1)*(SECT_SIZE)-5,&last_blk_offs,sizeof(int));
		return size;
	}

	//Append Mode (if content is less than the available space in the current sector)
	else if(how_much_write>=xs && fh->o_flags==33793){
		printf("APPENDING\n");
		memcpy(fs+METADATA+last_blk*(SECT_SIZE)+last_blk_offs,buf,(int)xs);
		last_blk_offs = last_blk_offs + (int)xs;
		fh->node->st_size += xs;
		memcpy(fs+BLOCKSIZE+fh->node->st_id*INODE_SIZE,fh->node,INODE_SIZE);	//again what is this????

		printf("HERE minsize %zu\n",fh->node->st_size);
		printf("HERE offset set to %d\n",last_blk_offs);
		memcpy(fs+METADATA+(last_blk+1)*(SECT_SIZE)-4,&last_blk_offs,sizeof(int));  //Update Offset Info
		return size;
	}


	int no_of_sectors_req;
	if( fh->o_flags==33793){
		no_of_sectors_req = (int)ceil((xs - how_much_write)/252.0);
	}
	else{
		no_of_sectors_req = (xs/252);  //ceil
	}
	printf("No of sects required are %d \n",no_of_sectors_req);
	int res = get_req_sects((int)node->st_id,no_of_sectors_req);		////////////////////////////////////////////////////////////
	if(res==-1){
		errno = EFBIG;
		return -errno;
	}
	printf("SECTOR METADATA LOOKS LIKE \n");
	memcpy(sector,fs+sector_ptr,sizeof(struct sector));
	memcpy(node,fs+BLOCKSIZE+(node->st_id)*INODE_SIZE,INODE_SIZE);
	for(int u=0;u<node->sector_tuples*2-1;u+=2){
		printf("\t Sector %d Offset %d\n",sector->tuples[u],sector->tuples[u+1]);
	}

	////////////////////////////////////////////////// forgot where to use last_blk
	int i=0,ilimit=0;
	if(fh->o_flags==33793){
		// last_blk = sector->tuples[node->sector_tuples*2-2] + sector->tuples[node->sector_tuples*2-1] - 1;
		last_blk = for_later;
		i=sector->tuples[for_later]+for_later2-1;
	}
	else{
		i = sector->tuples[0];
	}
	ilimit=sector->tuples[node->sector_tuples*2-2]+sector->tuples[node->sector_tuples*2-1];

	int rsize = xs;
	int end = 0,h=0,offo=0,flag = 0,ternary=0;
	//remember to increment i in the loop (i is the sector number)

	for(i ; i < ilimit ;){
		int start = i;
		int howmany = sector->tuples[for_later+1]-for_later2;	//for_later is the index of previously last tuple's first sector

		if(fh->o_flags==33793){
			printf("APPENDING EXCESS %d\n",i);				//for later= initial sector of last tuple
			memcpy(&offo,fs+METADATA+(start+1)*SECT_SIZE-4,sizeof(int));
			ternary=rsize>(256-offo-5)?(256-offo-5) : rsize;
			printf("Ternary %d H %d start=%d offo=%d \n",ternary,h,start,offo);
			memcpy(fs+METADATA+(start)*SECT_SIZE+offo,buf,ternary);
			h += ternary;						//h is how much of buf is written
			printf("h=%d\n",h);				
			int chitti=251;
			ternary+=offo;
			printf("ternary=%d\n",ternary);
			memcpy(fs+METADATA+(start+1)*SECT_SIZE-4,&ternary,sizeof(int));
			fh->node->st_size += ternary;
			memcpy(fs+BLOCKSIZE+fh->node->st_id*INODE_SIZE,fh->node,INODE_SIZE);
			rsize-=h;						//decremented h from full buf size
			printf("rsize1=%d howmany=%d\n",rsize,howmany);			
			if(howmany>0)
				start++;
			
			else{
				if(ceil((for_later+1)/2)<node->sector_tuples){
					for_later+=2;
					start=sector->tuples[for_later];
					howmany=sector->tuples[for_later+1];
				}
			}
			for(int j=start;j<start+howmany;j++){		
				start = j;
				printf("start=%d\n",start);
				int should_write = 0;
	
				//int offo = *(int*)(fs+METADATA+(start+1)*SECT_SIZE-4);  //Offset remove +1
				//int can_write = SECT_SIZE - offo - 5;    // Can write this much into this sector
				if(rsize>251)
					should_write = 251;
				else
					should_write = rsize;

				rsize = rsize - should_write;
				printf("RSIZE IS %d\n",rsize);
			
				memcpy(fs+METADATA+(start)*SECT_SIZE,buf+h,should_write);
				memcpy(fs+METADATA+(start+1)*SECT_SIZE-4,&should_write,sizeof(int));
				fh->node->st_size += should_write;
				printf("At sector number %d offset of sector %d h=%d\n",start,should_write,h);
				printf("Wrote+remaining %s\n",buf+h);
				memcpy(fs+BLOCKSIZE+fh->node->st_id*INODE_SIZE,fh->node,INODE_SIZE);
				h = h + should_write;
			}
			if((int)ceil((for_later+1)/2.0)<node->sector_tuples){
				for_later+=2;
				printf("if of append %d\n",for_later);
				i=sector->tuples[for_later];
				for_later2=1;		//only first time for_later value was required
			}
			else
				i+=2;		
		}

		else{
			printf("WRITING EXCESS i=%d\n",i);
			for(int j=0;j<=howmany;j++){
				//start = start + j;
				int should_write = 0;
				//int offo = *(int*)(fs+METADATA+(start+1)*SECT_SIZE-4);  //Offset

				int can_write = 251;    // Can write this much into this sector
				if(rsize>=can_write)
					should_write = can_write;
				else
					should_write = rsize;

				rsize = rsize - should_write;
				
				printf("HEREEEEEEEEEEEEEEE");
				
				memcpy(fs+METADATA+(start+j)*SECT_SIZE,buf+h,(int)should_write);	//h=0 initially
				memcpy(fs+METADATA+(start+1+j)*SECT_SIZE-4,&should_write,sizeof(int));
				//fh->node->st_size += fh->node->st_size>(int)strlen(buf)?should_write:0;
				fh->node->st_size += should_write;
				memcpy(fs+BLOCKSIZE+fh->node->st_id*INODE_SIZE,fh->node,INODE_SIZE);
				h = h + should_write;
				i++;
				
			}
			if((int)ceil((for_later+1)/2.0)<node->sector_tuples){
				for_later+=2;
				i=sector->tuples[for_later];
				for_later2=1;		//only first time for_later value was required
			}
			else
				i+=1;	
		}
		
				//SECT_SIZE=256
	}
	printf("16th sector\n");
	for(int k=0;k<280;k++)
		printf("%c",*(char *)(fs+METADATA+16*(SECT_SIZE)+k));
	printf("\n");

	//printf("SIZE is %lu\n",size);
	memcpy(sector,fs+sector_ptr,sizeof(struct sector));
	for(int u=0;u<node->sector_tuples*2-1;u+=2){
		printf("\t Sector %d Offset %d\n",sector->tuples[u],sector->tuples[u+1]);
	}
	free(sector);
	free(node);
	return size;
}


static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("fs_read called offset is %zu\n",offset);
	
	printf("16th sector\n");
	for(int k=0;k<320;k++)
		printf("%c",*(char *)(fs+METADATA+16*(SECT_SIZE)+k));
	printf("\n");

	struct filehandle *fh = (struct filehandle *) fi->fh;
	off_t filesize = fh->node->st_size;
	int sect_ptr = fh->node->sector_pointer;
	int sect_tups = fh->node->sector_tuples;

	int end = 0;
	struct sector * current_sector = (struct sector *)malloc(sizeof(struct sector));
	memcpy(current_sector,fs+sect_ptr,sizeof(struct sector));

	//extra tuples not considered here
	int how_much_read = 0;
	for(int i=0;i<sect_tups*2;i+=2){
		int start = current_sector->tuples[i];
		int offs = current_sector->tuples[i+1];
		printf("%d %d START OFFS\n",start,offs);
		for(int j=0;j<offs;j++){
			
			//start = start + j;
			printf("Sector  %d",start+j);
			int offo = 0;
			memcpy(buf+end,(char *)(fs+METADATA+(start+j)*(SECT_SIZE)),SECT_SIZE-5);
			memcpy(&offo,fs+METADATA+(start+j+1)*SECT_SIZE-4,sizeof(int));
			how_much_read = how_much_read + offo;
			//printf("%s STRLEN\n",buf);
			end = end + offo;
			printf("Offo %d end %d\n",offo,end);
		}
	}
	//for(int k=0;k<100;k++)
	//	printf("%d %c\n",k,*(char *)(fs+METADATA+97*(SECT_SIZE)+k));
	printf("16th sector\n");
	for(int k=0;k<320;k++)
		printf("%c",*(char *)(fs+METADATA+16*(SECT_SIZE)+k));
	printf("\n");
	set_time(fh->node, AT);
	return size;
}



static int fs_truncate(const char *path, off_t offset){
	printf("fs_truncate called.File is to be made of size =%ld\n",offset);



	struct myinode *node = (struct myinode *)malloc(sizeof(struct myinode));

	if(!getnodebypath(path, root, node)) {

	return -EEXIST;
	}

	if(offset==0)
	{
	struct sector* sec=(struct sector *)malloc(sizeof(struct sector));
	memcpy(sec,fs+node->sector_pointer,sizeof(struct sector));
	int startt;
	memcpy(&startt,fs+node->sector_pointer,sizeof(int));
	printf("Current sector pointer contents:%d\n",startt);
	for(int j=0;j<21;j++)
	printf("%d ",*((int *)(fs+(node->sector_pointer+j*sizeof(int)))));
	char *zero=(char *)calloc(256,sizeof(char)); 
	//memset(zero,'0',256);
	memset(zero,0,256);
	printf("printing array which I'm going to use for memcpy");
	printf("%s",zero);
	printf("\n");
	for(int i=0;i<node->sector_tuples*2;i+=2){
		int start = sec->tuples[i];
		int offs = sec->tuples[i+1];
		printf("%d %d START OFFS\n",start,offs);
		for(int k=0;k<251;k++)
			printf("%c",*(char *)(fs+METADATA+start*(SECT_SIZE)+k));
		printf("\n");
		 
	
		for(int j=0;j<offs;j++)
		
			memcpy(fs+METADATA+(start+j)*(SECT_SIZE),zero,SECT_SIZE);
		//memcpy(fs+METADATA+(start)*(SECT_SIZE),zero,offs*SECT_SIZE);
		printf("after making it 0\n");
		for(int k=0;k<256;k++)
			printf("%c",*(char *)(fs+METADATA+(start)*(SECT_SIZE)+k));
		printf("\n");
		}
		//going to make sector meta data -1
	
	//memcpy(&node->sector_pointer,); what should this be after deleting?
	for(int j=0;j<node->sector_tuples*2;j++){
		sec->tuples[j]=-1;
	
	}
	sec->extratuples=0;
	node->sector_tuples=0; 
	memcpy(fs+node->sector_pointer, sec, SECTOR_SIZE);
	printf("Current sector pointer contents after deleting\n");
	for(int j=0;j<21;j++)
	printf("%d ",*((int *)(fs+node->sector_pointer+j*sizeof(int))));

	}


	char secempty='0', secfull='1';



	int SEC = 0;
		  int s;
		  for(s=96; s<1440; s++) {
			if(memcmp(fs+2*BLOCKSIZE+s*sizeof(char), &secempty, sizeof(char))==0) {

			  SEC = s-80;
			  break;
			}
		  }
		  int one=1;
	  memcpy(fs+2*BLOCKSIZE+(SEC+80), &secfull, sizeof(char));
		  node->sector_tuples=1;
		  memcpy(fs+node->sector_pointer, &SEC, sizeof(int));
		  memcpy(fs+node->sector_pointer+sizeof(int), &one, sizeof(int));
		  printf("fs+sector_pointer : %d\n", *(int *)(fs+node->sector_pointer));
		  printf("Going to print data bitmap:\n");
		  for(int k=0;k<1440;k++)
		  	printf("%c ",*(char *)(fs+2*BLOCKSIZE+k));

	struct stat *stbuf=(struct stat *)malloc(sizeof(struct stat));
	node->st_size=offset;
	memcpy(fs+BLOCKSIZE+node->st_id*INODE_SIZE,node,INODE_SIZE);
	set_stat(node,stbuf);
	
	printf("new size is :%ld\n",stbuf->st_size);
	struct fuse_file_info *fi=(struct fuse_file_info *)malloc(sizeof(struct fuse_file_info));
	struct filehandle *fh =(struct filehandle*) malloc(sizeof(struct filehandle));
	fh->node=node;
	fi->fh = (uint64_t) fh;



	return 0;
}



/*
static int fs_chmod(const char* path, mode_t mode){


//get node by path
//print out current mode
struct myinode *node = (struct myinode *)malloc(sizeof(struct myinode));

if(!getnodebypath(path,root,node))
    
    return -errno;
   
struct stat *stbuf=(struct stat *)malloc(sizeof(struct stat));

printf("chmod called\n");
printf("Current file permissions are:%o\n",node->st_mode);

//now change permissions and update it in inode 
node->st_mode=mode;

stat(path,stbuf);

set_mode(node, stbuf);

//printf("kernel stat permissions:%o\n",stbuf->st_mode);
printf("New permissions are:%o\n",node->st_mode);
struct fuse_file_info *fi=(struct fuse_file_info *)malloc(sizeof(struct fuse_file_info));
struct filehandle *fh = malloc(sizeof(struct filehandle));
fh->node=node;
fi->fh = (uint64_t) fh;
printf("kernel stat permissions:%o\n",stbuf->st_mode);
//printf("%d",errno);


}
*/

static struct fuse_operations fs_oper = {
	.getattr = fs_getattr,
	.mkdir = fs_mkdir,
	.rmdir = fs_rmdir,
	.readdir = fs_readdir,
	.create = fs_creat,
	.open  = fs_open,
	.read  = fs_read,
	.write  = fs_write,
	.mknod = fs_mknod,
	.unlink =fs_unlink,
	.truncate=fs_truncate
	//.chmod=fs_chmod
};







//-------------MAIN------------------------------------
int main(int argc, char *argv[]) {
	// mtrace();

	fs = malloc(BLOCK_NO*BLOCKSIZE);
	memset(fs, 0, BLOCK_NO*BLOCKSIZE);

	// Initialize root directory
	root = (struct myinode *)malloc(sizeof(struct myinode));
	super = (struct superblock *)malloc(sizeof(struct superblock));
	memset(root, 0, INODE_SIZE);

	openfile();
	printf("outside openfile\n");
	set_time(root, AT|CT|MT);
	printf("outside set_time\n");
	umask(0);//default umask is to be used
	printf("umask\n");
	return fuse_main(argc, argv, &fs_oper, NULL);

	// return 1;
}
