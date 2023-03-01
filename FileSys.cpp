// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#include <cstring>
#include <iostream>
#include <unistd.h>
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
  dirblock_t currentTempBlock;
  bfs.read_block(curr_dir, (void*) &currentTempBlock);

  //check if name exceeds limit
  if(strlen(name) > MAX_FNAME_SIZE)  {
    //NAME EXCEEDS MAX FILE NAME SIZE ERROR
    return -1;
  }

  //check if max directory entries exceeded
  if(currentTempBlock.num_entries >= MAX_DIR_ENTRIES) {
    //MAX DIR ENTRIES EXCEEDED ERROR
    return -1;
  }

  //check if name already exists - linear search
  for(int i = 0; i < currentTempBlock.num_entries; i++)  {
    if(strcmp(currentTempBlock.dir_entries[i], name))  {//checks if dir already exists
      //DIRECTORY ALREADY EXISTS ERROR
      return -1;
    }
  }
  

  short blockID = bfs.get_free_block();
  if(blockID <= 0)  { //check that given block is not superblock
    //CANNOT GET FREE BLOCK ERROR
    return -1;
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
  currentTempBlock.dir_entries[currentTempBlock.num_entries].block_num = blockID;
  strcpy(currentTempBlock.dir_entries[currentTempBlock.num_entries].name, name);
  currentTempBlock.num_entries++;

  

  //write blocks that were changed back to disk
  bfs.write_block(curr_dir, (void*) &currentTempBlock);
  bfs.write_block(blockID, (void*) &newDir);

}

// switch to a directory
void FileSys::cd(const char *name)
{
  dirblock_t tempDir;
  dirblock_t cdDir;
  bfs.read_block(curr_dir, (void*) &tempDir);

  for(int i = 0; i < tempDir.num_entries; i++)  {//scan through entries in current directory
    if(strcmp(tempDir.dir_entries[i], name))  {//checks if target directory exists in current directory
      read_block(tempDir.entries[i].block_num, (void*) &cdDir); //loads block to check block type
      if(cdDir.magic == DIR_MAGIC_NUM)  { //checks if block loaded is a Directory block
        curr_dir = tempDir.entries[i].block_num;// sets current directory to new block
      }
    }
  }

  //DIRECTORY NOT FOUND ERROR
  return -1;
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
    if(strcmp(tempDir.dir_entries[i], name))  {//checks if target directory exists in current directory
      read_block(tempDir.entries[i].block_num, (void*) &cdDir); //loads block to check block type
      if((cdDir.magic == DIR_MAGIC_NUM))  { //checks if block loaded is a Directory block
        if(cdDir.num_entries == 0)  {
        //found target directory

          //give back disk block
          bfs.reclaim_block(cdDir.block_num)
          
          //update parent directory metadata
          tempDir.entries[i].num_entries--;
          for(int x = i; x <= tempDir.num_entries; x++)  {
            tempDir.dir_entries[x] = tempDir.dir_entries[x + 1];
          }
          
          //write block with changes to disk
          write_block(tempDir.block_num, (*void) &tempDir);
        }
        else  {
          //DIRECTORY NOT EMPTY ERROR
          return -1;
        }
      }
    }
  }

  //ERROR DIRECTORY NOT FOUND
  return -1;
}

// list the contents of current directory
void FileSys::ls()
{
  dirblock_t currBlock;
  dirblock_t tempBlock;

  bfs.read_block(curr_dir, &currBlock);// read current directory

  for(int i = 0; i < currBlock.num_entries; i++)  {//iterates through block entries
    bfs.read_block(currBlock.dir_entries[i], &tempBlock); //reads block to check meta data

    if(tempBlock.magic == DIR_MAGIC_NUM)  {//entry is a directory
      cout << tempBlock.dir_entries[i].name << "/" << endl;
    }
    else if(tempBlock.magic == INODE_MAGIC_NUM) {//entry is a inode
      cout << tempBlock.dir_entries[i].name << "/" << endl;
    }
    else  {
      //UNKNOWN BLOCK TYPE ERROR
      return -1;
    }
  }

}

// create an empty data file
void FileSys::create(const char *name)
{
  dirblock_t currBlock;

  bfs.read_block(curr_dir. &currBlock);

  //check if name exceeds limit
  if(strlen(name) > MAX_FNAME_SIZE)  {
    //NAME EXCEEDS MAX FILE NAME SIZE ERROR
    return -1;
  }

  //check if name already exists - linear search
  for(int i = 0; i < currBlock.num_entries; i++)  {
    if(strcmp(currBlock.dir_entries[i], name))  {//checks if dir already exists
      //DIRECTORY ALREADY EXISTS ERROR
      return -1;
    }
  }

  //check if max directory entries exceeded
  if(currBlock.num_entries >= MAX_DIR_ENTRIES) {
    //MAX DIR ENTRIES EXCEEDED ERROR
    return -1;
  }
  //all precondtions passed, now make inode for file
  
  //get new block to hold inode in current directory
  short newBlock = bfs.get_free_block();
  if(blockID <= 0)  { //check that given block is not superblock
    //CANNOT GET FREE BLOCK ERROR
    return -1;
  }


  
  //create inode and setup meta data
  struct inode_t newFileInode;
  newFileInode.magic = INODE_MAGIC_NUM;
  newFileInode.size = 0;
  for (int z = 0; z < MAX_DATA_BLOCKS; z++){
    newFileInode.blocks[z] = 0;
  }

  //update parent meta data 
  currBlock.size++;
  currBlock.dir_entries[size - 1].block_num = newBlock;
  strcpy(currBlock.dir_entries[size - 1].name, name);


  //write changed blocks to disk
  bfs.write_block(curr_dir, (void*) currBlock);
  bfs.write_block(newBlock, (void*) newFileInode);


}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{
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

