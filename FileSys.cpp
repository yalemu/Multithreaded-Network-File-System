// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <cmath>
using namespace std;

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"

// mounts the file system
void FileSys::mount(int sock) {
  bfs.mount();
  curr_dir = 1; //by default current directory is home directory, in disk block #1
  fs_sock = sock; //use this socket to receive file system operations from the client and send back response messages
}

// unmounts the file system
void FileSys::unmount() {
  bfs.unmount();
  close(fs_sock);
}

// make a directory
void FileSys::mkdir(const char *name)
{
  dirblock_t currenttempInodeBlock;
  bfs.read_block(curr_dir, (void*) &currenttempInodeBlock);

  //check if name exceeds limit
  if(strlen(name) > MAX_FNAME_SIZE)  {
    //NAME EXCEEDS MAX FILE NAME SIZE ERROR
    return;
  }

  //check if max directory entries exceeded
  if(currenttempInodeBlock.num_entries >= MAX_DIR_ENTRIES) {
    //MAX DIR ENTRIES EXCEEDED ERROR
    return;
  }

  //check if name already exists - linear search
  for(int i = 0; i < currenttempInodeBlock.num_entries; i++)  {
    if(strcmp(currenttempInodeBlock.dir_entries[i].name, name) == 0)  {//checks if dir already exists
      //DIRECTORY ALREADY EXISTS ERROR
      return;
    }
  }
  

  short blockID = bfs.get_free_block();
  if(blockID <= 0)  { //check that given block is not superblock
    //CANNOT GET FREE BLOCK ERROR
    return;
  }


  //setup new directory 
  struct dirblock_t newDir; 

  //set new dir metadata
  newDir.magic = DIR_MAGIC_NUM; 
  newDir.num_entries = 0; 
  for(int j = 0; j < MAX_DIR_ENTRIES; j++)  { //initialize all dir entries to empty
    newDir.dir_entries[j].block_num = 0;
  }


  //update parent directory meta data
  currenttempInodeBlock.dir_entries[currenttempInodeBlock.num_entries].block_num = blockID;
  strcpy(currenttempInodeBlock.dir_entries[currenttempInodeBlock.num_entries].name, name);
  currenttempInodeBlock.num_entries++;

  

  //write blocks that were changed back to disk
  bfs.write_block(curr_dir, (void*) &currenttempInodeBlock);
  bfs.write_block(blockID, (void*) &newDir);

}

// switch to a directory
void FileSys::cd(const char *name)
{
  dirblock_t tempDir;
  dirblock_t cdDir;
  bfs.read_block(curr_dir, (void*) &tempDir);

  for(int i = 0; i < tempDir.num_entries; i++)  {//scan through entries in current directory
    if(strcmp(tempDir.dir_entries[i].name, name) == 0)  {//checks if target directory exists in current directory
      bfs.read_block(tempDir.dir_entries[i].block_num, (void*) &cdDir); //loads block to check block type
      if(cdDir.magic == DIR_MAGIC_NUM)  { //checks if block loaded is a Directory block
        curr_dir = tempDir.dir_entries[i].block_num;// sets current directory to new block
      }
    }
  }

  //DIRECTORY NOT FOUND ERROR
  return;
}

// switch to home directory
void FileSys::home() {
  curr_dir = 1; //set directory block to home directory block
}

// remove a directory
void FileSys::rmdir(const char *name)
{
  dirblock_t tempDir;
  dirblock_t cdDir;
  bfs.read_block(curr_dir, (void*) &tempDir);

  for(int i = 0; i < tempDir.num_entries; i++)  {//scan through entries in current directory
    if(strcmp(tempDir.dir_entries[i].name, name) == 0)  {//checks if target directory exists in current directory
      bfs.read_block(tempDir.dir_entries[i].block_num, (void*) &cdDir); //loads block to check block type
      if((cdDir.magic == DIR_MAGIC_NUM))  { //checks if block loaded is a Directory block
        if(cdDir.num_entries == 0)  { //ensure that directory is empty
        //found target directory

          //give back disk block
          bfs.reclaim_block(tempDir.dir_entries[i].block_num);
          
          //update parent directory metadata
          tempDir.num_entries--;
          for(int x = i; x <= tempDir.num_entries; x++)  {
            tempDir.dir_entries[x] = tempDir.dir_entries[x + 1];
          }
          
          //write block with changes to disk
          bfs.write_block(curr_dir, (void*) &tempDir);
        }
        else  {
          //DIRECTORY NOT EMPTY ERROR
          return;
        }
      }
    }
  }

  //ERROR DIRECTORY NOT FOUND
  return;
}

// list the contents of current directory
void FileSys::ls()
{
  dirblock_t currBlock;
  dirblock_t tempInodeBlock;

  bfs.read_block(curr_dir, &currBlock);// read current directory

  for(int i = 0; i < currBlock.num_entries; i++)  {//iterates through block entries
    bfs.read_block(currBlock.dir_entries[i].block_num, &tempInodeBlock); //reads block to check meta data

    if(tempInodeBlock.magic == DIR_MAGIC_NUM)  {//entry is a directory
      cout << tempInodeBlock.dir_entries[i].name << "/" << endl;
    }
    else if(tempInodeBlock.magic == INODE_MAGIC_NUM) {//entry is a inode
      cout << tempInodeBlock.dir_entries[i].name << "/" << endl;
    }
    else  {
      //UNKNOWN BLOCK TYPE ERROR
      return;
    }
  }

}

// create an empty data file
void FileSys::create(const char *name)
{
  dirblock_t currBlock;

  bfs.read_block(curr_dir, &currBlock);

  //check if name exceeds limit
  if(strlen(name) > MAX_FNAME_SIZE)  {
    //NAME EXCEEDS MAX FILE NAME SIZE ERROR
    return;
  }

  //check if name already exists - linear search
  for(int i = 0; i < currBlock.num_entries; i++)  {
    if(strcmp(currBlock.dir_entries[i].name, name) == 0)  {//checks if dir already exists
      //DIRECTORY ALREADY EXISTS ERROR
      return;
    }
  }

  //check if max directory entries exceeded
  if(currBlock.num_entries >= MAX_DIR_ENTRIES) {
    //MAX DIR ENTRIES EXCEEDED ERROR
    return;
  }
  //all precondtions passed, now make inode for file
  
  //get new block to hold inode in current directory
  short newBlock = bfs.get_free_block();
  if(newBlock <= 0)  { //check that given block is not superblock
    //CANNOT GET FREE BLOCK ERROR
    return;
  }


  
  //create inode and setup meta data
  struct inode_t newFileInode;
  newFileInode.magic = INODE_MAGIC_NUM;
  newFileInode.size = 0;
  for (int z = 0; z < MAX_DATA_BLOCKS; z++){
    newFileInode.blocks[z] = 0;
  }

  //update parent meta data 
  currBlock.num_entries++;
  currBlock.dir_entries[currBlock.num_entries - 1].block_num = newBlock;
  strcpy(currBlock.dir_entries[currBlock.num_entries - 1].name, name);


  //write changed blocks to disk
  bfs.write_block(curr_dir, (void*) &currBlock);
  bfs.write_block(newBlock, (void*) &newFileInode);


}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{
  int appDataSize = strlen(data);

  if(appDataSize == 0)  { //no point in continuing 
    return NULL;
  }

  dirblock_t currBlock;
  inode_t tempInodeBlock;
  datablock_t tempDataBlock;

  int totalNewBlocks = 0;
  int currentDataBlock = 0;
  int currentDataBytesLeft = 0;
  int totNeededBlocks = 0;
  int dataBookmark = 0;

  int freeBlockNum = 0;
  bfs.read_block(curr_dir, &currBlock);// read current directory

  for(int i = 0; i < currBlock.num_entries; i++)  {//iterates through block entries
    if(strcmp(currBlock.dir_entries[i].name, name) == 0)  {//checks if names match
      bfs.read_block(currBlock.dir_entries[i].block_num, &tempInodeBlock); //reads block to check meta data 
      if(tempInodeBlock.magic == INODE_MAGIC_NUM) {//entry is a inode

        //file target found
        currentDataBlock = ceil(static_cast<double>(tempInodeBlock.size) / BLOCK_SIZE); //find total number of used blocks
        nextDataBytesLeft = (tempInodeBlock.size - (currentDataBlock - 1)*BLOCK_SIZE); //find if there is/amount of free bytes in last data block
        totNeededBlocks = ceil(static_cast<double>(appDataSize + tempInodeBlock.size) / BLOCK_SIZE);

        if((tempInodeBlock.size + appDataSize) > MAX_FILE_SIZE)  {//check if append would execeed MAX_FILE_SIZE
          
          if(currentDataBytesLeft != 0)  {//if there is already blocks of data, maybe need to be filled

            bfs.read_block(tempInodeBlock.blocks[currentDataBlock - 1], &tempDataBlock);
            for(int z = nextDataBytesLeft; ((nextDataBytesLeft < BLOCK_SIZE) || (data[dataBookmark] != '\0')))  { //fill up last blocks empty space 
              tempDataBlock[z] = data[dataBookmark];
              dataBookmark++;
            }
            
            freeBlockNum = bfs.get_free_block();
            if(newBlock <= 0)  { //check that given block is not superblock
              //CANNOT GET FREE BLOCK ERROR
              return;
            }

            tempInodeBlock.blocks[currentDataBlock - 1] = freeBlockNum; //put new block into inode
            bfs.write_block(freeBlockNum, &tempDataBlock);  //write filled data block with to disk

            totalNewBlocks = totNeededBlocks - currentDataBlock;
          }
          else {//fresh append, with prev blocks already full of stored data
            if(appDataSize <= 128)  { //allocate 1 block for data
              totalNewBlocks = 1;
            }
            else  { //need to allocate multiple blocks
              totalNewBlocks = ceil(static_cast<double>(appDataSize) / BLOCK_SIZE);
            }
          }

          datablock_t* dataArr = new datablock_t[totalNewBlocks];
          for(int v = 0; v < totalNewBlocks; v++) {
            for(t = 0; t < BLOCK_SIZE; t++) { //store data into datablock
              dataArr[v].data[t] = data[dataBookmark];
              dataBookmark++; //point to next char in data
            }
            freeBlockNum = bfs.get_free_block();
            if(newBlock <= 0)  { //check that given block is not superblock
              //CANNOT GET FREE BLOCK ERROR
              return;
            }
            tempInodeBlock.blocks[currentDataBlock] = freeBlockNum; //put new block into inode
            bfs.write_block(freeBlockNum, &dataArr[v]);  //write new block with data block info to disk
            currentDataBlock++; //update what block is needed to be updated next
          }

          //update inode metadata
          tempInodeBlock.size = tempInodeBlock.size + appDataSize;
          bfs.write_block(currBlock.dir_entries[i].block_num, &tempInodeBlock);


        }
        else  { 
          //MAX FILE LIMIT WILL BE REACHED WITH APPEND
          return;
        }

        tempInodeBlock.size = tempInodeBlock.size + appDataSize;

      }
      else  {
        //NOT A DIRECTORY
        return;
      }
    }     
  }

  //CANNOT FIND TARGET FILE
  return;
}

// display the contents of a data file
void FileSys::cat(const char *name)
{
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n)
{
}

// delete a data file
void FileSys::rm(const char *name)
{
}

// display stats about file or directory
void FileSys::stat(const char *name)
{
}

// HELPER FUNCTIONS (optional)

