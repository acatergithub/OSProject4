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
bitmap_t inodeBitmap;
bitmap_t dataBitmap; 
int inodeBitmapNum;
int dataBitmapNum;
int numBlocks;//DISK_SIZE/BLOCKSIZE
int inodeSize;
void * buffer;
int inodesPerBlock;//Calculate in init
int startOfInode;
int startOfData;
struct superblock SuperBlock;
int dirPerBlock;
struct dirent * rootDir;
struct inode * dirInode;

int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 
	bio_read(inodeBitmapNum,buffer);
	inodeBitmap = *(bitmap_t *) buffer;
	
	for(int i = 0; i < inodeSize; i++){
		if(get_bitmap(inodeBitmap,i)==0){
			//inode index i is free
			set_bitmap(inodeBitmap,i);
			*(bitmap_t *) buffer = inodeBitmap;
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

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 

	bio_read(dataBitmapNum,buffer);
	dataBitmap = *(bitmap_t *) buffer;
	for(int i = 0; i < numBlocks; i++){
		if(get_bitmap(dataBitmap,i)==0){
			//dataBitmap index i is free
			set_bitmap(dataBitmap,i);
			*(bitmap_t *) buffer = dataBitmap;
			bio_write(dataBitmapNum,dataBitmap);
			return i;
		}
	}
	//No free data was found
	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number

  // Step 2: Get offset of the inode in the inode on-disk block

  // Step 3: Read the block from disk and then copy into inode structure

	//First Calculate Block Number
	int blockNum = ino/inodesPerBlock;
	blockNum = blockNum + startOfInode;
	int offset = ino%inodesPerBlock;
	void * temp;
	//offset and block number are calculated
	bio_read(blockNum, buffer);
	temp = &buffer + (offset *(sizeof(inode)));
	//Buffer now points to innode within buffer 
	inode = (struct inode *)temp;

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk 

	int blockNum = ino/inodesPerBlock;
	blockNum = blockNum + startOfInode;
	int offset = ino%inodesPerBlock;
	void * temp;
	//offset and block number are calculated
	bio_read(blockNum, buffer);
	temp = &buffer + (offset *(sizeof(inode)));
	//Buffer now points to innode within buffer 
	*(struct inode *)temp = *inode;//May cause problems MARK
	bio_write(blockNum,buffer);

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode * tempNode = 0; 
	readi(ino, tempNode);
	struct dirent * tempDir;
	bool match = true;
  // Step 2: Get data block of current directory from inode
	for(int i = 0; i < dirPerBlock; i++){
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
	}
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
	if(match){
		return 1;
	}
	else{
		return 0;
	}
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent * tempDir = 0;
	bool match = true;
	int newBlock = -1;
	struct dirent * firstOpen = 0;
	for(int i = 0; i < 16;i++){//HARD CODED CHANGE LATER: Iterates through all inode blocks
		if(dir_inode.direct_ptr[i]!=0){
			for(int j = 0; j < dirPerBlock; j++){
				tempDir = (struct dirent *)(&dir_inode.direct_ptr[i] +(sizeof(struct dirent)* i));
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
				else if(firstOpen == 0){
					firstOpen = tempDir;
				}
			}
		}
		else if(firstOpen == 0){
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
		dir_inode.direct_ptr[newBlock] = block;
		for(int i = 0; i < name_len;i++){
			tempBlock[0].name[i] = fname[i];
		}
		tempBlock[0].len = name_len;
		tempBlock[0].ino = f_ino;
		int curBlock = startOfData + newBlock;
		bio_write(curBlock,tempBlock);
	}
	

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	//Asuming that root inode is passed. If not replace inode with rootInode
	printf("we got here1\n");
	bool found = false;
	int sizeOfPath = sizeof(path)/sizeof(*path);
	char * next = (char *)malloc(sizeof(char *)*sizeOfPath);
	int counter = 1;
	int j = 0;
	struct inode * tempNode = inode;
	bool match = false;
	printf("we got here2\n");
	while (!found){
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
	}
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile

	// write superblock information

	// initialize inode bitmap

	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory
	//come back to this later MARK
	dev_init(diskfile_path);
	struct superblock bigBlock;
	bigBlock.magic_num = MAGIC_NUM;
	bigBlock.max_inum = MAX_INUM;
	bigBlock.max_dnum = MAX_DNUM;
	bigBlock.i_bitmap_blk = 1;
	int blocks_for_imap = (MAX_INUM*(sizeof(struct inode))/BLOCK_SIZE);
	bigBlock.d_bitmap_blk = 2 + blocks_for_imap;
	int blocks_for_dmap = MAX_DNUM/BLOCK_SIZE;
	bigBlock.i_start_blk = 3 + blocks_for_dmap;
	int blocks_for_inode = MAX_INUM/(sizeof(struct inode));
	bigBlock.d_start_blk = bigBlock.i_start_blk + blocks_for_inode;

	bio_write(0,&bigBlock);
	inodeBitmap = 0;
	set_bitmap(inodeBitmap,0);
	set_bitmap(inodeBitmap,1);
	//IMPLEMENT: Root Directory config

	set_bitmap(inodeBitmap,2);
	dirInode= (struct inode *)malloc(sizeof(struct inode));
	dirInode->ino = 2;
	rootDir->ino = 2;
	rootDir->len = sizeof(diskfile_path)/sizeof(*diskfile_path);
	for(int i = 0; i <rootDir->len; i++){
		rootDir->name[i] = diskfile_path[i];
	}
	dirInode->ino = 2;
	dirInode->valid = 1;
	dirInode->size = 1;//CHANGE WHAT IS THIS?

	
	bio_write(bigBlock.i_bitmap_blk,inodeBitmap);
	dataBitmap = 0;
	bio_write(bigBlock.d_bitmap_blk,dataBitmap);
	dirPerBlock = BLOCK_SIZE/sizeof(struct dirent);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	if(dev_open(diskfile_path)==-1){
		//No file of that name exists
		tfs_mkfs();
	}
	else{
		bio_read(0,&SuperBlock);
		bio_read(SuperBlock.i_bitmap_blk,&inodeBitmap);
		bio_read(SuperBlock.d_bitmap_blk,&dataBitmap);
		//CHANGE: May need more stuff
	}
	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode * tempNode = 0;
	uint16_t tempNum = 0;
	get_node_by_path(path, tempNum, tempNode);
	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);
	tempNode->vstat = *stbuf;
	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1
	struct inode * node = 0;
	uint16_t ino = 0;
	if(get_node_by_path(path, ino, node) == -1){
		printf("Shits not real idiot");
		return -1;
	}
	else{
		if(node->valid==1){
			return 0;
		}
	}
    return -1;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode * tempNode = 0;
	int inoNum = 0;
	get_node_by_path(path, inoNum, tempNode);
	struct dirent * tempDir = 0;
	inoNum = tempNode->ino;

	for(int i = 0; i < 16; i++){//HARD CODED CHANGE LATER
		if(tempNode->direct_ptr[i] != 0){
			for(int j = 0; j < dirPerBlock; j++){
				bio_read(tempNode->direct_ptr[i],buffer);
				tempDir = (struct dirent *)(&buffer + (sizeof(struct dirent) * i));
				filler(buffer,tempDir->name, NULL,0);
			}
		}
	}
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	
	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char * dirName = dirname((char *)path);
	char * baseName = basename((char *) path);
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode * tempNode = 0;
	int inoNum = 0;
	get_node_by_path(dirname, inoNum, tempNode);
	// Step 3: Call get_avail_ino() to get an available inode number
	inoNum = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(*tempNode, inoNum, basename,(sizeof(baseName)/sizeof(*baseName)));
	// Step 5: Update inode for target directory
	//FIX THIS
	// Step 6: Call writei() to write inode to disk
	writei(inoNum,tempNode); 

	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char * dirName = dirname((char *)path);
	char * baseName = basename((char *) path);
	struct inode * tempNode = 0;
	int inoNum = 0;
	// Step 2: Call get_node_by_path() to get inode of target directory
	get_node_by_path((char *)path, inoNum, tempNode);


	// Step 3: Clear data block bitmap of target directory
	for(int i = 0; 1 < 16; i++){
		set_bitmap(dataBitmap,tempNode->direct_ptr[i]);
		tempNode->direct_ptr[i] = 0;
	}
	bio_write(SuperBlock.d_bitmap_blk, dataBitmap);
	// Step 4: Clear inode bitmap and its data block
	set_bitmap(inodeBitmap, tempNode->ino);
	bio_write(SuperBlock.i_bitmap_blk,inodeBitmap);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	get_node_by_path(dirName, inoNum, tempNode);
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	dir_remove(*tempNode, baseName,(sizeof(baseName)/sizeof(*baseName)));
	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char * dirName = dirname((char *)path);
	char * baseName = basename((char *) path);
	struct inode * tempNode = 0;
	int inoNum = 0;
	// Step 2: Call get_node_by_path() to get inode of parent directory
	get_node_by_path(dirName, inoNum, tempNode);
	// Step 3: Call get_avail_ino() to get an available inode number
	inoNum = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(*tempNode, inoNum, baseName, sizeof(baseName)/sizeof(*baseName));
	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk
	writei(inoNum, tempNode);
	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1
	int inoNum = 0;
	struct inode * tempNode = 0;
	return get_node_by_path(path, inoNum, tempNode);
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

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

void get_node_from_num(uint16_t ino, struct inode * inode){
	int block = ino/inodesPerBlock;
	int offset = ino%inodesPerBlock;
	bio_read(block,buffer);
	*inode = *(struct inode *)(buffer + (offset*(sizeof(inode)))); 
}

int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

