/*
 *  Copyright (C) 2021 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here

/* 
 * Get available inode number from bitmap
 */
bitmap_t  inodeBitmap;
bitmap_t  dataBitmap; 
int inodeBitmapNum;
int dataBitmapNum;
int numBlocks;//DISK_SIZE/BLOCKSIZE
int inodeSize;
void * buffer;
int inodesPerBlock;//Calculate in init
int startOfInode;
int startOfData;
int dirPerBlock;
struct superblock * superBlock;

struct dirent * rootDir;
struct inode * dirInode;

int get_avail_ino() {
	//printf("ENTERING get_avail_ino\n");
	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 
	bio_read(inodeBitmapNum,buffer);
	inodeBitmap = (bitmap_t)buffer;
	//printf("THIS SHOULD PRINT 1: IT PRINTS %d\n",get_bitmap(inodeBitmap, 2));
	
	for(int i = 0; i < MAX_INUM; i++){
		if(get_bitmap(inodeBitmap,i)==0){
			//inode index i is free
			set_bitmap(inodeBitmap,i);
			buffer = inodeBitmap;
			//printf("BIO WRITING AT BLOCK %d\n",inodeBitmapNum);
			bio_write(inodeBitmapNum,inodeBitmap);
			return i;
		}
	}
	//No free inode was found
	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
	//printf("ENTERING get_avail_blk\n");
	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 

	bio_read(dataBitmapNum,buffer);
	dataBitmap = (bitmap_t) buffer;
	for(int i = 0; i < numBlocks; i++){
		//printf("\nLOOKING AT BLOCK %d, on dataBitMap: %d\n",i,get_bitmap(dataBitmap,i));
		if(get_bitmap(dataBitmap,i)==0){
			//dataBitmap index i is free
			set_bitmap(dataBitmap,i);
			memcpy(buffer,dataBitmap,BLOCK_SIZE);
			//("BIO WRITING AT BLOCK %d\n",dataBitmapNum);
			bio_write(dataBitmapNum,dataBitmap);
			//printf("get_avail_blkno returning: %d", (i+startOfData));
			return i+startOfData;
		}
	}
	//No free data was found
	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
//	printf("ENTERING readi\n");
  // Step 1: Get the inode's on-disk block number

  // Step 2: Get offset of the inode in the inode on-disk block

  // Step 3: Read the block from disk and then copy into inode structure

	//First Calculate Block Number
	int blockNum = ino/inodesPerBlock;
	blockNum = blockNum + startOfInode;
	int offset = (ino%inodesPerBlock)*sizeof(struct inode);
	
	//offset and block number are calculated
	bio_read(blockNum, buffer);
	struct inode * tempNode =  (struct inode *)(buffer + offset);
	//Buffer now points to innode within buffer 
	memcpy(inode,tempNode,sizeof(struct inode));

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {
	//printf("ENTERING writei\n");
	inode_dump("Ewritei");
	// Step 1: Get the block number where this inode resides on disk
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk 

	int blockNum = ino/inodesPerBlock;
	blockNum = blockNum + startOfInode;
	int offset = (ino%inodesPerBlock)*sizeof(struct inode);
	struct inode * temp;
	//offset and block number are calculated
	bio_read(blockNum, buffer);
	temp = (struct inode *)(buffer + offset);
	//Buffer now points to innode within buffer 
	//*temp = *inode;//May cause problems MARK
	memcpy(temp,inode,sizeof(struct inode));
	//printf("BIO WRITING INODE AT BLOCK %d, OFFSET %d\n",blockNum,offset);
	//printf("INODE INO %d\n",temp->ino);
	bio_write(blockNum,buffer);

	//bio_read(blockNum,buffer);
	//temp = (struct inode *)(buffer + 512);

	//printf("CHECK TO SEE IF OTHER NODES STILL EXIST: %d\n", temp->ino);
	inode_dump("Xwritei");
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	//printf("ENTERING dir_find\n");
  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode * tempNode = (struct inode *)malloc(sizeof(struct inode)); 
	readi(ino, tempNode);
	struct dirent * tempDir;//POSSIBLE ERROR
	bool match = false;
  // Step 2: Get data block of current directory from inode
  if(tempNode != 0){
	 //printf("Got here 1: %s\n", fname);
	  for(int i = 0; i < 16; i++){
		  //printf("Got here 1i: %d, %d\n", i, tempNode->direct_ptr[i]);
		  if(tempNode->direct_ptr[i] != 0){
			  bio_read(tempNode->direct_ptr[i], buffer);
			  for(int j = 0; j < dirPerBlock; j++){
				  tempDir = (struct dirent *)(buffer + (sizeof(dirent)*i));
				  //printf("Got here 2: %s, %s\n", tempDir->name, fname);
				  if(tempDir != 0)
				  {
					match = true;
					for(int k = 0; k < name_len;k++){
						if(tempDir->name[k] != fname[k]){
							match = false;
							break;
						}
				  	}
					if(match){
						//printf("Got here 3\n");
						memcpy(dirent, tempDir, sizeof(struct dirent));
						//printf("Got here 4\n");
						if (tempNode) free(tempNode);
						return 0;
					}   
				  }
			  }
		  }
	  }
  }
  if (tempNode) free(tempNode);
  return -ENOENT;
  
  
	/*for(int i = 0; i < dirPerBlock; i++){
		tempDir = (struct dirent *)(tempNode->direct_ptr[0] + (sizeof(struct dirent)*i));//CHECK SCALE
		if(tempDir != 0){
			if(tempDir->len == name_len){
				match = true;
				for(int j = 0; j < name_len; j++){
					if(tempDir->name[j]!=fname[j]){
						match = false;
						break;
					}
				}
				if(match){
					dirent = tempDir;
					break;
				}
			}
		}
	}*/

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	//printf("ENTERING dir_add\n");
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	struct dirent * tempDir = 0;
	int newBlock = -1;
	int oldBlock = -1;
	int offset = -1;
	struct dirent * newDir;
	for(int i = 0; i < 16;i++){//HARD CODED CHANGE LATER: Iterates through all inode blocks
		if(dir_inode.direct_ptr[i]!=0){
			bio_read(dir_inode.direct_ptr[i], buffer);
			for(int j = 0; j < dirPerBlock; j++){
				tempDir = (struct dirent *)(buffer +(sizeof(struct dirent)* i));
					if(tempDir->len == name_len){
						// Step 2: Check if fname (directory name) is already used in other entries
						if(strcmp(tempDir->name, fname)==0){
							return -EEXIST; //ERNO FILE EXISTS ERROR REPLACE
						}
					}
					else if(tempDir->len == 0 && offset == -1){
						offset = (sizeof(struct dirent)* j);
						oldBlock = i;
					}
			}
		}
		else if(newBlock == -1){
			newBlock = i;
		}
	}
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	if(oldBlock != -1){//Block exists and has space
		//printf("ADDING WITH OLD BLOCK");
	//	printf("ADDING TO INODE %d",dir_inode.ino);
		bio_read(dir_inode.direct_ptr[oldBlock],buffer);
		newDir = (struct dirent *)(buffer + offset);
		newDir->len = name_len;
		strcpy(newDir->name,fname);
		//printf("NAME OF NEW DIRENT %s\n",newDir->name);
		newDir->ino = f_ino;
		newDir->valid = 1;
		//printf("BIO WRITING IN DIR_ADD AT BLOCK %d\n",dir_inode.direct_ptr[oldBlock]);
		bio_write(dir_inode.direct_ptr[oldBlock],buffer);
		//New Dirent is created, initilized, and stored in memory.
	}
	else if(newBlock != -1){// Allocate a new data block for this directory if it does not exist
	//	printf("ADDING WITH NEW BLOCK");
	//	printf("ADDING TO INODE %d",dir_inode.ino);
		int freeBlock = get_avail_blkno();
		dir_inode.direct_ptr[newBlock] = freeBlock;// Update directory inode
		writei(dir_inode.ino, &dir_inode);
		bio_read(freeBlock,buffer);
		newDir = (struct dirent *)(buffer);
		newDir->len = name_len;
		strcpy(newDir->name,fname);
		newDir->ino = f_ino;
		newDir->valid = 1;
	//	printf("BIO WRITING AT BLOCK %d\n", freeBlock);
		bio_write(freeBlock,buffer);
	}
	else{
		return -1; //FULL
	}
	inode_dump("Xdir_add");
	// Write directory entry
	//create_inode(newDir->ino, 1, BLOCK_SIZE);//Creates new inode

	return 0;
	/*struct dirent * tempDir = 0;
	bool match = true;
	int newBlock = -1;
	struct dirent * firstOpen = 0;
	for(int i = 0; i < 16;i++){//HARD CODED CHANGE LATER: Iterates through all inode blocks
		if(dir_inode.direct_ptr[i]!=0){
			bio_read(dir_inode.direct_ptr[i],buffer);
			for(int j = 0; j < dirPerBlock; j++){
				tempDir = (struct dirent *)(buffer +(sizeof(struct dirent)* i));
				if(tempDir != 0){
					if(tempDir->len == name_len){
						match = true;
						for(int k = 0; k < name_len;k++){
							if(tempDir->name[k] != fname[k]){
								match = false;
								break;
							} 
						}
						if(match){
							return -1;
						}
					}
				}
				else{
					newBlock = i;
				}
			}
		}
		else if(newBlock == 0){
			newBlock = i;
		}

	}
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	if(firstOpen != 0){
		for(int i = 0; i < name_len;i++){
			firstOpen->name[i] = fname[i];
		}
		firstOpen->len = name_len;
		firstOpen->ino = f_ino;
		//int curBlock = startOfInode + dir_inode.ino/inodesPerBlock;
		
	}
	else{// Allocate a new data block for this directory if it does not exist
		int block=0;
		block =	get_avail_blkno();
		if(block == -1){
			return -1;
		}
		struct dirent * tempBlock = (struct dirent *)malloc(sizeof(struct dirent)*dirPerBlock);
		// Update directory inode
		dir_inode.direct_ptr[newBlock] = block;
		for(int i = 0; i < name_len;i++){
			tempBlock[0].name[i] = fname[i];
		}
		

		// Write directory entry
		tempBlock[0].len = name_len;
		tempBlock[0].ino = f_ino;
		int curBlock = startOfData + newBlock;
		printf("BIO WRITING AT BLOCK %d\n",curBlock);
		bio_write(curBlock,tempBlock);
	}
	

	*/
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {
	//	printf("ENTERING dir_remove\n");
	struct dirent * tempDir;
	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	for(int i = 0; i < 16; i++){
		if(dir_inode.direct_ptr[i] != 0){
			bio_read(dir_inode.direct_ptr[i],buffer);
			for(int j = 0; j < dirPerBlock; j++){// Step 2: Check if fname exist
				tempDir = (struct dirent *)(buffer + (sizeof(struct dirent)*j));
				if(strcmp(tempDir->name,fname)==0){
					// Step 3: If exist, then remove it from dir_inode's data block and write to disk
					//bio_read(inodeBitmapNum,buffer);
					//inodeBitmap = (bitmap_t)buffer;
					//set_bitmap(inodeBitmap,tempDir->ino);
					//bio_write(inodeBitmapNum, buffer);
					//printf("\nClearing Dirent Ino: %d in DataBlock: %d\n",tempDir->ino,dir_inode.direct_ptr[i]);
					memset(tempDir,0,sizeof(struct dirent));
					
					bio_write(dir_inode.direct_ptr[i],buffer);
					inode_dump("Xdir_remove");
					return 0;
				}
			}
		}
	}
	
	return -ENOENT;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	//	printf("ENTERING get_node_by_path\n");
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	//Asuming that root inode is passed. If not replace inode with rootInode
	//("we got here1: %s\n",path);
	//bool found = false;
	int sizeOfPath = strlen(path);
	char * next = (char *)malloc(sizeof(char)*(sizeOfPath + 1));
	int counter = 0;
	int errCode = 0;
	//struct inode * tempNode = (struct inode *)malloc(sizeof(struct inode));
	//bool match = false;
	struct dirent * tempDir = (struct dirent *)malloc(sizeof(struct dirent));
	//int blockNum = ino/BLOCK_SIZE + startOfInode;
	//int offset = ino%BLOCK_SIZE;
	//bio_read(blockNum, buffer);
	//tempNode = (struct inode *)(buffer + (sizeof(struct inode *)*offset));
	if(sizeOfPath < 2){
		readi(ino, inode);
		//printf("RETURNING: INODE %d\n", inode->ino);
		if (tempDir) free(tempDir);
		if (next) free(next);
		return 0;
	}
	for(int i = 1; i < sizeOfPath && path[i] != '/'; i++){
		next[i-1] = path[i];
		next[i] = '\0'; // add NULL character
		counter++;
	}
	counter++;
	//printf("we got here2: %s\n", next);
	int length = strlen(next);
	if((errCode = dir_find(ino, next, length, tempDir)) == 0){
		//cont
		next[0] = '\0';
		next[1] = '\0';
		for(int i = 0; counter < sizeOfPath; i++){
			next[i] = path[counter];
			next[i+1] = '\0'; // add NULL character
			counter++;
		}
		//printf("NEXT IS NOW %s\n",next);
		//printf("we got here3: %s\n", next);
		int error;
		error = get_node_by_path(next,tempDir->ino,inode);
		if (next) free(next);
		if (tempDir) free(tempDir);
		return error;
	}
	else {
		//inode->ino = ino;
		if (next) free(next);
		if (tempDir) free(tempDir);
		return errCode;
	}
	
	/*if(path[1]=='\000'){
		inode = dirInode;
		return 0;
	}
	while(!found){
		while(counter < sizeOfPath && path[counter]!='/'&& path[counter]!='0'){
			next[x] = path[counter];
			counter++;
			x++;
		}
		blockNum = tempDir->ino/(inodesPerBlock) + startOfInode;
		offset = tempDir->ino%(inodesPerBlock);
		bio_read(blockNum, buffer);
		tempNode = (struct inode *) (&buffer + (sizeof(struct inode)*offset));
		
		for(int i = 0; i < 16; i++){//HARD CODED CHANGE LATER
			if(tempNode != 0 && tempNode->direct_ptr[i] != 0){
					for(int j = 0; j < dirPerBlock; j++){
						bio_read(tempNode->direct_ptr[i], buffer);
						tempDir = (struct dirent *)(&buffer + (sizeof(struct dirent) * i));
						if(tempDir->len == sizeOfPath){
							for(int k = 0; k < sizeOfPath; k++){
								if(next[k] != tempDir->name[k]){
									match = false;
									break;
								}
							}
							if(match){
								if(path[counter] == '0'){
									blockNum = tempDir->ino/(inodesPerBlock) + startOfInode;
									offset = tempDir->ino%(inodesPerBlock);
									bio_read(blockNum, buffer);
									tempNode = (struct inode *) (&buffer + (sizeof(struct inode)*offset));
									ino = tempDir->ino;
									return 1;
								}
								else{
									blockNum = tempDir->ino/(inodesPerBlock) + startOfInode;
									offset = tempDir->ino%(inodesPerBlock);
									bio_read(blockNum, buffer);
									tempNode = (struct inode *) (&buffer + (sizeof(struct inode)*offset));
									ino = tempDir->ino;
									break;
								}
							}
						}
				}
				if(match){
					break;
				}
			}
		}
		if(!match){
			return -1;
		}
	}
	*/
	/*while (!found){
		printf("Iteration %d\n",counter);
		j = 0;
		
		while(counter < sizeOfPath && path[counter]!='/'&& path[counter]!='0'){
			next[j] = path[counter];
			counter++;
			j++;
		}
		printf("Passed loop 1");
		counter++;
		struct dirent * tempDir = 0;
		for(int i = 0; i < dirPerBlock; i++){
			printf("Entering For Loop 1");
			tempDir = (struct dirent *)(&tempNode->direct_ptr[0] + (sizeof(struct dirent) * i));
			if(tempDir != 0 && tempDir->len == j){
				match = true;
				printf("Entering For Loop 2");
				for(int x = 0; x < j; x++){
					if(next[x] != tempDir->name[x]){
						match = false;
						break;
					}
				}
				if(match){
					get_node_from_num(tempDir->ino, tempNode);
					break;
				}
			}
		}
		if(tempNode->type==1){//1 for non directory CHECK
			if(path[counter] == '0'){
				found  = true;
			}
		}
	}
	if(found){
		inode = tempNode;
		ino = tempNode->ino;
		return 0;
	}
	else{
		return -1;
	}*/
	if (next) free(next);
	if (tempDir) free(tempDir);
	return -1;
}

/* 
 * Make file system
 */
int tfs_mkfs() {
	//printf("ENTERING tfs_mkfs\n");
	// Call dev_init() to initialize (Create) Diskfile

	// write superblock information

	// initialize inode bitmap

	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory
	//come back to this later MARK
	dev_init(diskfile_path);
	superBlock = (struct superblock *)malloc(sizeof(struct superblock));
	superBlock->magic_num = MAGIC_NUM;
	superBlock->max_inum = MAX_INUM;
	superBlock->max_dnum = MAX_DNUM;
	superBlock->i_bitmap_blk = 1;
	int blocks_for_imap = (MAX_INUM/BLOCK_SIZE/(sizeof(bitmap_t)));
	superBlock->d_bitmap_blk = 2 + blocks_for_imap;
	int blocks_for_dmap = MAX_DNUM/BLOCK_SIZE/(sizeof(bitmap_t));
	superBlock->i_start_blk = 3 + blocks_for_dmap;
	int blocks_for_inode = MAX_INUM/(sizeof(struct inode));
	superBlock->d_start_blk = superBlock->i_start_blk + blocks_for_inode;
	inodeBitmap = (bitmap_t)malloc(MAX_INUM/(sizeof(bitmap_t)));
	//printf("BIO WRITING AT BLOCK %d\n",0);
	bio_write(0,superBlock);
	set_bitmap(inodeBitmap,0);
	//printf("Bitmap set to %d\n", get_bitmap(inodeBitmap,0));
	set_bitmap(inodeBitmap,1);
	//printf("Bitmap set to %d\n",get_bitmap(inodeBitmap,0));
	//IMPLEMENT: Root Directory config

	set_bitmap(inodeBitmap,2);
	dirInode= (struct inode *)malloc(sizeof(struct inode));
	rootDir = (struct dirent *)malloc(sizeof(struct dirent));
	memset(dirInode,0,sizeof(struct inode));
	memset(rootDir,0,sizeof(struct dirent));
	dirInode->ino = 2;
	rootDir->ino = 2;
	//rootDir->len = sizeof(diskfile_path)/sizeof(*diskfile_path);
	//for(int i = 0; i <rootDir->len; i++){
	//	rootDir->name[i] = diskfile_path[i];
	//}
	rootDir->len = 1;
	strcpy(rootDir->name,"/");
	dirInode->ino = 2;
	dirInode->valid = 1;
	dirInode->type = 1;
	buffer = (void *)malloc(BLOCK_SIZE);
	struct inode * tempPoint = (struct inode *)(buffer + (sizeof(struct inode)*2));
	memcpy(tempPoint,dirInode,sizeof(struct inode));
	//printf("BIO WRITING AT BLOCK %d\n",superBlock->i_start_blk);
	bio_write(superBlock->i_start_blk,buffer);
	//dirInode->size = 1;//CHANGE WHAT IS THIS?
	//printf("%d\n",tempPoint->ino);
	//printf("BIO WRITING AT BLOCK %d\n",superBlock->i_bitmap_blk);
	bio_write(superBlock->i_bitmap_blk,inodeBitmap);
	dataBitmap = (bitmap_t)malloc(MAX_DNUM/(sizeof(bitmap_t)));
	//printf("BIO WRITING AT BLOCK %d\n",superBlock->d_bitmap_blk);
	bio_write(superBlock->d_bitmap_blk,dataBitmap);

	inodeBitmapNum = superBlock->i_bitmap_blk;
	dataBitmapNum = superBlock->d_bitmap_blk;
	dirPerBlock = BLOCK_SIZE/sizeof(struct dirent);
	numBlocks = MAX_DNUM;//DISK_SIZE/BLOCKSIZE
	inodesPerBlock = BLOCK_SIZE/sizeof(struct inode);//Calculate in init
	startOfInode = superBlock->i_start_blk;
	startOfData = superBlock->d_start_blk;
	
	bio_read(inodeBitmapNum,buffer);
	bitmap_t tempinodeBitmap = (bitmap_t)buffer;
	//printf("THIS SHOULD PRINT 1: IT PRINTS %d\n",get_bitmap(tempinodeBitmap, 2));

	/*
	int inodeBitmapNum;
	int dataBitmapNum;
	int numBlocks;//DISK_SIZE/BLOCKSIZE
	int inodeSize;
	void * buffer;
	int inodesPerBlock;//Calculate in init
	int startOfInode;
	int startOfData;
	int dirPerBlock;
	*/
	inode_dump("Xtfs_mkfs");
	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
	//	printf("ENTERING tfs_init\n");
	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	if(dev_open(diskfile_path)==-1){
		//No file of that name exists
		tfs_mkfs();
	}
	else{
		superBlock = (struct superblock *)malloc(sizeof(struct superblock));
		inodeBitmap = (bitmap_t)malloc(MAX_INUM/(sizeof(bitmap_t)));
		dirInode= (struct inode *)malloc(sizeof(struct inode));
		rootDir = (struct dirent *)malloc(sizeof(struct dirent));
		dataBitmap = (bitmap_t)malloc(MAX_DNUM/(sizeof(bitmap_t)));
		buffer = (void *)malloc(BLOCK_SIZE);
		bio_read(0,buffer);
		superBlock = (struct superblock *)(buffer);
		bio_read(superBlock->i_bitmap_blk,buffer);
		inodeBitmap = (bitmap_t)(buffer);
		bio_read(superBlock->d_bitmap_blk,buffer);
		dataBitmap = (bitmap_t)(buffer);
		rootDir->len = 1;
		rootDir->name[0] = '/';
		rootDir->ino = 2;
		dirInode->ino = 2;
		dirInode->valid = 1;
		inodeBitmapNum = superBlock->i_bitmap_blk;
		dataBitmapNum = superBlock->d_bitmap_blk;
		dirPerBlock = BLOCK_SIZE/sizeof(struct dirent);
		numBlocks = MAX_DNUM;//DISK_SIZE/BLOCKSIZE
		inodesPerBlock = BLOCK_SIZE/sizeof(struct inode);//Calculate in init
		startOfInode = superBlock->i_start_blk;
		startOfData = superBlock->d_start_blk;


		bio_read(inodeBitmapNum,buffer);
		bitmap_t tempinodeBitmap = (bitmap_t)buffer;
		//printf("THIS SHOULD PRINT 1: IT PRINTS %d\n",get_bitmap(tempinodeBitmap, 2));
		//CHANGE: May need more stuff
	}
	inode_dump("Xtfs_init");
	return NULL;
}

static void tfs_destroy(void *userdata) {
	//	printf("ENTERING tfs_destroy\n");
	// Step 1: De-allocate in-memory data structures
	inodeBitmap =0;
	dataBitmap=0; 
	inodeBitmapNum=0;
	dataBitmapNum=0;
	numBlocks=0;//DISK_SIZE/BLOCKSIZE
	inodeSize=0;
	buffer=0;
	inodesPerBlock=0;//Calculate in init
	startOfInode=0;	
	startOfData=0;
	dirPerBlock=0;
	superBlock=0;
	rootDir=0;
	dirInode=0;
	// Step 2: Close diskfile
	dev_close();
}

static int tfs_getattr(const char *path, struct stat *stbuf) {
	//	printf("ENTERING tfs_getattr\n");
	// Step 1: call get_node_by_path() to get inode from path
	int errCode = 0;
	struct inode * tempNode;
	tempNode = (struct inode *) malloc(sizeof(struct inode));
	uint16_t tempNum = rootDir->ino;
	if((errCode = get_node_by_path(path, tempNum, tempNode))!=0)
		//printf("NO NODE RETURNED");
		return errCode;

	// Step 2: fill attribute of file into stbuf from inode
	//stbuf->st_mode   = S_IFDIR | 0755;
	stbuf->st_atime = tempNode->vstat.st_atime;
	stbuf->st_mtime = tempNode->vstat.st_mtime;
	stbuf->st_ctime = tempNode->vstat.st_ctime;
	stbuf->st_mode  = tempNode->vstat.st_mode;
	stbuf->st_nlink = tempNode->vstat.st_nlink;
	stbuf->st_ino = tempNode->ino;
	stbuf->st_uid = tempNode->vstat.st_uid;
	stbuf->st_gid =  tempNode->vstat.st_gid;
	stbuf->st_blksize = tempNode->vstat.st_blksize;
	stbuf->st_size = tempNode->vstat.st_size;
	stbuf->st_blocks = tempNode->vstat.st_blocks;

	if (tempNode) free(tempNode);
	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {
	//printf("ENTERING tfs_opendir\n");
	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1
	struct inode * node = (struct inode *)malloc(sizeof(struct inode));
	uint16_t ino = rootDir->ino;
	if(get_node_by_path(path, ino, node) != -1){
		if (node) free(node);
		return  0;
	}
	else{
		//printf("Shits not real idiot\n");
		if (node) free(node);
		return -1;
	}
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	//printf("ENTERING tfs_readdir\n");
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode * tempNode = (struct inode *)malloc(sizeof(struct inode));
	int inoNum = rootDir->ino;
	get_node_by_path(path, inoNum, tempNode);
	struct dirent * tempDir = 0;
	inoNum = tempNode->ino;
	readi(tempNode->ino,tempNode);
	for(int i = 0; i < 16; i++){//HARD CODED CHANGE LATER
		if(tempNode->direct_ptr[i] != 0){
			for(int j = 0; j < dirPerBlock; j++){
				bio_read(tempNode->direct_ptr[i],buffer);
				tempDir = (struct dirent *)(buffer + (sizeof(struct dirent) * i));
				filler(buffer,tempDir->name, NULL,0);
			}
		}
	}
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	if (tempNode) free(tempNode);
	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {
//printf("ENTERING tfs_mkdir\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char * dirName = dirname((char *)path);
	char * baseName = basename((char *) path);
	inode_dump("Entering tfs_mkdir");
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode tempNode;
	int inoNum = rootDir->ino;
	get_node_by_path((const char *)dirName, inoNum, &tempNode);
	// Step 3: Call get_avail_ino() to get an available inode number
	inoNum = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
//	printf("ASDJFDHJSAFHJSDHFJDHJHFHJSDF %d",tempNode.ino);
	//printf("jkdjjdfjjjdjdjjdjdsjjdjdjjdjdjjdjdjdj %d",inoNum);
	//dir_add(tempNode, inoNum, (const char *)basename,strlen(baseName));
	inode_dump("Middle of tfs_mkdir");
	// Step 5: Update inode for target directory
	//FIX THIS
	// Step 6: Call writei() to write inode to disk
	writei(inoNum,&tempNode); 
	inode_dump("End of tfs_mkdir");
	return 0;
}

static int tfs_rmdir(const char *path) {
//	printf("ENTERING tfs_rmdir\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char * dirName = dirname((char *)path);
	char * baseName = basename((char *) path);
	struct inode tempNode;
	int inoNum = 0;
	// Step 2: Call get_node_by_path() to get inode of target directory
	get_node_by_path((char *)path, inoNum, &tempNode);


	// Step 3: Clear data block bitmap of target directory
	for(int i = 0; 1 < 16; i++){
		set_bitmap(dataBitmap,tempNode.direct_ptr[i]);
		tempNode.direct_ptr[i] = 0;
	}
	//printf("BIO WRITING AT BLOCK %d",superBlock->d_bitmap_blk);
	bio_write(superBlock->d_bitmap_blk, dataBitmap);
	// Step 4: Clear inode bitmap and its data block
	set_bitmap(inodeBitmap, tempNode.ino);
	//printf("BIO WRITING AT BLOCK %d",superBlock->i_bitmap_blk);
	bio_write(superBlock->i_bitmap_blk,&inodeBitmap);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	get_node_by_path(dirName, inoNum, &tempNode);
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	dir_remove(tempNode, baseName,(sizeof(baseName)/sizeof(*baseName)));
	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	//printf("ENTERING tfs_create\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	int sizeOfPath = strlen(path);
	//printf("NAME OF PATH: %s\n",path);
	char * dirName = strdup(path);
	char * baseName = strdup(path);
	strncpy(dirName,path,sizeOfPath);
	strncpy(baseName,path,sizeOfPath);
	
	dirName = dirname(dirName);
	baseName = basename(baseName);
	//printf("NAME OF NEW DIRENT: %s\n",baseName);
	struct inode tempNode;
	int inoNum = rootDir->ino;
	// Step 2: Call get_node_by_path() to get inode of parent directory
	get_node_by_path(dirName, inoNum, &tempNode);
	// Step 3: Call get_avail_ino() to get an available inode number
	inoNum = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(tempNode, inoNum, baseName, strlen(baseName));
	// Step 5: Update inode for target file
	tempNode.ino = inoNum;
	tempNode.type = 0; //don't think this is right
	tempNode.size = 0;
	tempNode.link = 0;
	tempNode.valid = 1;
	time(&tempNode.vstat.st_atime);
	time(&tempNode.vstat.st_mtime);
	time(&tempNode.vstat.st_ctime);
	tempNode.vstat.st_uid = getuid();
	tempNode.vstat.st_gid = getgid();
	tempNode.vstat.st_ino = inoNum;
	tempNode.vstat.st_nlink = 0;
	tempNode.vstat.st_mode =  mode;
	tempNode.vstat.st_size = 0;
	tempNode.vstat.st_blksize = BLOCK_SIZE;
	tempNode.vstat.st_blocks = 0;
	// Step 6: Call writei() to write inode to disk
	writei(inoNum, &tempNode);
	//if (baseName) free(baseName);
	//if (dirName) free(dirName);
	inode_dump("Xtfs_create");
	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {
	//printf("ENTERING tfs_open\n");
	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1
	int inoNum = rootDir->ino;
	struct inode * tempNode = (struct inode *)malloc(sizeof(struct inode));
	int error = get_node_by_path(path, inoNum, tempNode);
	if (tempNode) free(tempNode);
	return error;
}

static int tfs_read(const char *path, char *outputbuffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	//printf("ENTERING tfs_read\n");
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode * tempNode = (struct inode *)malloc(sizeof(struct inode));
	get_node_by_path(path,rootDir->ino,tempNode);
	int inoNum = rootDir->ino;
	int error;
	if((error = get_node_by_path(path,inoNum,tempNode))!=0){
		return error;
	}
	// Step 2: Based on size and offset, read its data blocks from disk
	if(size == 0){
		return 0;
	}
	int blocksNeeded;
	int startBlock = (offset)/BLOCK_SIZE;
	int curBlock = startBlock;
	int totalBytesRead = 0;
	int startOffset = offset%BLOCK_SIZE;
	int currentMax = 0;
	int bytesToRead = size;
	if(size%BLOCK_SIZE == 0){
		blocksNeeded = (size + startOffset)/BLOCK_SIZE;
	}
	else{
		blocksNeeded = (size + startOffset)/BLOCK_SIZE + 1;
	}
	// Step 3: copy the correct amount of data from offset to buffer
	for(int i = 0; i < blocksNeeded; i++){
		curBlock = curBlock + i;
		if(tempNode->direct_ptr[curBlock] == 0){//No allocated block to write data, create new block.
			return -1;
		}
		if(BLOCK_SIZE - startOffset >= bytesToRead){
			bio_read(tempNode->direct_ptr[curBlock],buffer);
			memcpy(outputbuffer,(buffer + startOffset),bytesToRead);
			totalBytesRead = totalBytesRead + bytesToRead;

		}
		else{
			currentMax = BLOCK_SIZE - startOffset;
			bio_read(tempNode->direct_ptr[curBlock],buffer);
			memcpy(outputbuffer,(buffer + startOffset),currentMax);
			totalBytesRead = totalBytesRead + currentMax;
			bytesToRead = bytesToRead - currentMax;
			startOffset = 0;
		}
		
	}
	// Note: this function should return the amount of bytes you copied to buffer
	if (tempNode) free(tempNode);
	return totalBytesRead;
}

static int tfs_write(const char *path, const char *inputbuffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	
	/*
		Note! The size is not reset when loading from disk. This means that the size will be larger than expected causing a failure. Come back to this later
		if tfs_close does not solve this problem.
		pwrite()
	*/
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode * tempNode = (struct inode *)malloc(sizeof(struct inode));
	//printf("\nENTERING tfs_write\n");
	int inoNum = rootDir->ino;
	int error;
	if((error = get_node_by_path(path,inoNum,tempNode))!=0){
		return error;
	}
	//printf("\nWRTING TO INODE: %d\n", tempNode->ino);
	// Step 2: Based on size and offset, read its data blocks from disk
	if(size == 0){
		return 0;
	}
	int blocksNeeded;
	int startBlock = (offset)/BLOCK_SIZE;
	int curBlock = startBlock;
	int totalBytesWritten = 0;
	int bytesToWrite = size;
	int startOffset = offset%BLOCK_SIZE;
	int currentMax = 0;
	int newBlocks = 0;
	
	if(size%BLOCK_SIZE == 0){
		blocksNeeded = (size + startOffset)/BLOCK_SIZE;
	}
	else{
		blocksNeeded = (size + startOffset)/BLOCK_SIZE + 1;
	}
	//printf("WRITING %d BLOCKS\n", blocksNeeded);

	inode_dump("tfs_write1");
	for(int i = 0; i < blocksNeeded; i++){
		curBlock = curBlock + i;
		if(tempNode->direct_ptr[curBlock] == 0){//No allocated block to write data, create new block.
			tempNode->direct_ptr[curBlock] = get_avail_blkno();
			//printf("\nShould equal the return above: %d\n",tempNode->direct_ptr[curBlock]);
			newBlocks++;
			tempNode->vstat.st_size = tempNode->vstat.st_size + BLOCK_SIZE;
		}
		// Step 3: Write the correct amount of data from offset to disk
		
		if(BLOCK_SIZE - startOffset >= bytesToWrite){
			bio_read(tempNode->direct_ptr[curBlock],buffer);
			memcpy((buffer + startOffset),inputbuffer,bytesToWrite);
			//printf("\nBIO WRTING IN TFS_WRITE TO BLOCK: %d, %d bytes\n", tempNode->direct_ptr[curBlock],bytesToWrite);
			inode_dump("tfs_write2");
			bio_write(tempNode->direct_ptr[curBlock],buffer);
			inode_dump("tfs_write3");
			totalBytesWritten = totalBytesWritten + bytesToWrite;
		}
		else{
			currentMax = BLOCK_SIZE - startOffset;
			bio_read(tempNode->direct_ptr[curBlock],buffer);
			memcpy((buffer + startOffset),inputbuffer,currentMax);
			//printf("\nBIO WRTING IN TFS_WRITE TO BLOCK: %d, %d bytes\n", tempNode->direct_ptr[curBlock],currentMax);
			inode_dump("tfs_write4");
			bio_write(tempNode->direct_ptr[curBlock],buffer);
			inode_dump("tfs_write5");
			totalBytesWritten = totalBytesWritten + currentMax;
			bytesToWrite = bytesToWrite - currentMax;
			startOffset = 0;
		}
		
	}

	// Step 4: Update the inode info and write it to disk
	tempNode->size = tempNode->size + totalBytesWritten;
	
	tempNode->vstat.st_blksize = tempNode->vstat.st_blksize + newBlocks;
	//("WRITEI CALL WITH INO %d: \n", tempNode->ino);
	writei(tempNode->ino,tempNode);
	// Note: this function should return the amount of bytes you write to disk
	free(tempNode);
	//printf("\nBYTES WRITTEN THIS CALL: %d\n",totalBytesWritten);
	//printf("\nBYTES THAT SHOULD HAVE BEEN WRITEN THIS CALL: %ld\n",size);
	return totalBytesWritten;
}

static int tfs_unlink(const char *path) {

	//printf("ENTERING tfs_unlink\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	int sizeOfPath = strlen(path);
	//printf("NAME OF PATH: %s\n",path);
	char * dirName = strdup(path);
	char * baseName = strdup(path);
	strncpy(dirName,path,sizeOfPath);
	strncpy(baseName,path,sizeOfPath);
	dirName = dirname(dirName);
	baseName = basename(baseName);
	struct inode tempNode;
	int error;
	int inoNum;
	inode_dump("Entering Tfs_unlink");
	// Step 2: Call get_node_by_path() to get inode of target file
	if((error = get_node_by_path(path,rootDir->ino,&tempNode))!= 0){
		return error;
	}
	// Step 3: Clear data block bitmap of target file
	for(int i = 0; i < 16; i ++){
		if(tempNode.direct_ptr[i] != 0){
			//COME BACK LATER NOTHING IS ACTUALLY REMOVED
			memset(buffer,0,BLOCK_SIZE);
			//printf("CLEARING MEMEORY AT BLOCK: %d\n",tempNode.direct_ptr[i]);
			bio_write(tempNode.direct_ptr[i],buffer);
			
			bio_read(dataBitmapNum,buffer);
			dataBitmap = (bitmap_t)buffer;
			set_bitmap(dataBitmap, tempNode.direct_ptr[i]);
			//printf("BIO WRITING DATA BITMAP BACK TO DISK AT BLOCK %d:\n",dataBitmapNum);
			bio_write(dataBitmapNum,buffer);
			tempNode.direct_ptr[i] = 0;
		}
	}
	// Step 4: Clear inode bitmap and its data block
	inoNum = tempNode.ino;
	memset(&tempNode,0,sizeof(tempNode));
	writei(inoNum,&tempNode);//BIG CHANCE FOR ERROR
	
	bio_read(inodeBitmapNum,buffer);
	inodeBitmap = (bitmap_t)buffer;
	set_bitmap(inodeBitmap, inoNum);
	//printf("BIO WRITING INODE BITMAP BACK TO DISK AT BLOCK %d:",dataBitmapNum);
	bio_write(inodeBitmapNum,buffer);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	if((error = get_node_by_path(dirName, rootDir->ino,&tempNode))!=0){
		return error;
	}
	inode_dump("Middle of Tfs_unlink");
	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	if((error = dir_remove(tempNode,baseName,strlen(baseName)))!= 0){
		return error;
	}
	inode_dump("End of Tfs_unlink");
	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};

//void get_node_from_num(uint16_t ino, struct inode * inode){
//	int block = ino/inodesPerBlock;
//	int offset = ino%inodesPerBlock;
//	bio_read(block,buffer);
//	memcpy(inode, = *(struct inode *)(buffer + (offset*(sizeof(inode)))); 
//}
int create_inode(uint16_t ino, int type, int size){
	//creates and writes a inode of given type.
	//printf("ENTERING: create_inode"); 
	int block = ino/inodesPerBlock + startOfInode;
	int offset = ino%inodesPerBlock;
	struct inode * tempNode;
	bio_read(block, buffer);
	tempNode = (struct inode *)(buffer + offset);
	tempNode->ino = ino;
	tempNode->type = type;
	tempNode->size = size;
	//printf("BIO WRITING AT BLOCK %d\n",block);
	bio_write(block,buffer);
	return 0;
}
void inode_dump(char *comment){
	return 0; //Remove to continue test
	printf(">>> DUMPING THE INODE STRUCTURE: %s\n", comment);
	readi(2,dirInode);
	inode_dump_Helper(dirInode, comment);
}
void inode_dump_Helper(struct inode * tempNode, char *comment){
	printf(">> Printing inode: %d, type: %d, valid: %d\n",tempNode->ino, dirInode->type, dirInode->valid);

	struct inode * nextInode = (struct inode *)malloc(sizeof(struct inode));
	struct dirent * tempDir;
	for(int i = 0; i < 16;i++){
		if(tempNode->direct_ptr[i] != 0){
			printf(">> Inode: %d points to data block: %d\n",tempNode->ino,tempNode->direct_ptr[i]);
			bio_read(tempNode->direct_ptr[i],buffer);
			for(int j = 0; j < dirPerBlock; j++){
				tempDir = (struct dirent *)(buffer + (sizeof(struct dirent)*j));
				if(tempDir->ino != 0&& tempNode->type == 1){
					char name[208];
					strncpy(name,tempDir->name,20);
					name[20] = '\0';
					printf(">> Inode: %d contains a dirent for Inode: %d, Filename: %s. Now Entering Inode %d:\n",tempNode->ino,tempDir->ino,name,tempDir->ino);
					readi(tempDir->ino,nextInode);
					inode_dump_Helper(nextInode, "");
				}
			}
		}
	}
}
int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

