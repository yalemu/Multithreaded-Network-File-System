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
  dirblock_t currentDirectoryBlock;
  bfs.read_block(curr_dir, (void*) &currentDirectoryBlock);

  //check if name exceeds limit
  if(strlen(name) > MAX_FNAME_SIZE)  {
    //NAME EXCEEDS MAX FILE NAME SIZE ERROR
    return;
  }

  //check if max directory entries exceeded
  if(currentDirectoryBlock.num_entries >= MAX_DIR_ENTRIES) {
    //MAX DIR ENTRIES EXCEEDED ERROR
    return;
  }

  //check if name already exists - linear search
  for(int i = 0; i < currentDirectoryBlock.num_entries; i++)  {
    if(strcmp(currentDirectoryBlock.dir_entries[i].name, name) == 0)  {//checks if dir already exists
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
  currentDirectoryBlock.dir_entries[currentDirectoryBlock.num_entries].block_num = blockID;
  strcpy(currentDirectoryBlock.dir_entries[currentDirectoryBlock.num_entries].name, name);
  currentDirectoryBlock.num_entries++;

  

  //write blocks that were changed back to disk
  bfs.write_block(curr_dir, (void*) &currentDirectoryBlock);
  bfs.write_block(blockID, (void*) &newDir);

}

// switch to a directory
void FileSys::cd(const char *name)
{
  dirblock_t currentDirectory;
  dirblock_t targetDirectory;
  bfs.read_block(curr_dir, (void*) &currentDirectory); //read in current Directory

  for(int i = 0; i < currentDirectory.num_entries; i++)  {//scan through entries in current directory
    if(strcmp(currentDirectory.dir_entries[i].name, name) == 0)  {//checks if target directory exists in current directory
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void*) &targetDirectory); //loads block to check block type
      if(targetDirectory.magic == DIR_MAGIC_NUM)  { //checks if block loaded is a Directory block
        curr_dir = currentDirectory.dir_entries[i].block_num;// sets current directory to new block
      }
      else  {
        //NOT A DIRECTORY 
        return;
      }
    }
  }

  //DIRECTORY NOT FOUND ERROR
  return;
}

// switch to home directory
void FileSys::home() {
  curr_dir = 1; //set current directory block number to home directory block
}

// remove a directory
void FileSys::rmdir(const char *name)
{
  dirblock_t currentDirectory;
  dirblock_t cdDir;
  bfs.read_block(curr_dir, (void*) &currentDirectory);

  for(int i = 0; i < currentDirectory.num_entries; i++)  {//scan through entries in current directory
    if(strcmp(currentDirectory.dir_entries[i].name, name) == 0)  {//checks if target directory exists in current directory
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void*) &cdDir); //loads block to check block type
      if((cdDir.magic == DIR_MAGIC_NUM))  { //checks if block loaded is a Directory block
        if(cdDir.num_entries == 0)  { //ensure that directory is empty
        //found target directory

          //give back disk block
          bfs.reclaim_block(currentDirectory.dir_entries[i].block_num);
          
          //update parent directory metadata
          currentDirectory.num_entries--;
          for(int x = i; x <= currentDirectory.num_entries; x++)  {
            currentDirectory.dir_entries[x] = currentDirectory.dir_entries[x + 1];
          }
          
          //write block with changes to disk
          bfs.write_block(curr_dir, (void*) &currentDirectory);
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
  dirblock_t currentDirectory;
  dirblock_t tempInodeBlock;

  bfs.read_block(curr_dir, &currentDirectory);// read current directory

  for(int i = 0; i < currentDirectory.num_entries; i++)  {//iterates through block entries
    bfs.read_block(currentDirectory.dir_entries[i].block_num, &tempInodeBlock); //reads block to check meta data

    if(tempInodeBlock.magic == DIR_MAGIC_NUM)  {//entry is a directory
      cout << tempInodeBlock.dir_entries[i].name << "/" << endl;
    }
    else if(tempInodeBlock.magic == INODE_MAGIC_NUM) {//entry is a inode
      cout << tempInodeBlock.dir_entries[i].name << endl;
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
    return ;
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
        currentDataBytesLeft = (tempInodeBlock.size - (currentDataBlock - 1)*BLOCK_SIZE); //find if there is/amount of used bytes in last data block
        totNeededBlocks = ceil(static_cast<double>(appDataSize + tempInodeBlock.size) / BLOCK_SIZE);  //total blocks needed to handle append data

        if((tempInodeBlock.size + appDataSize) > MAX_FILE_SIZE)  {//check if append would execeed MAX_FILE_SIZE
          
          if(currentDataBytesLeft != 0)  {//if there is already blocks of data, maybe need to be filled

            bfs.read_block(tempInodeBlock.blocks[currentDataBlock - 1], &tempDataBlock);  //get last data block to append data to

            for(int z = currentDataBytesLeft; ((z < BLOCK_SIZE) || (data[dataBookmark] != '\0')); z++ )  { //fill up last blocks empty space 
              tempDataBlock.data[z] = data[dataBookmark];
              dataBookmark++;
            }
            
            //write updated block to memory
            freeBlockNum = bfs.get_free_block();
            if(freeBlockNum <= 0)  { //check that given block is not superblock
              //CANNOT GET FREE BLOCK ERROR
              return;
            }
            tempInodeBlock.blocks[currentDataBlock - 1] = freeBlockNum; 
            bfs.write_block(freeBlockNum, &tempDataBlock); 

            //how many new whole blocks needed to add rest of data
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

          //code to add whole new data blocks to append data to

          datablock_t* dataArr = new datablock_t[totalNewBlocks];
          for(int v = 0; v < totalNewBlocks; v++) {
            for(int t = 0; t < BLOCK_SIZE; t++) { //store data into datablock
              dataArr[v].data[t] = data[dataBookmark];
              dataBookmark++; //point to next char in data
            }
            freeBlockNum = bfs.get_free_block();
            if(freeBlockNum <= 0)  { //check that given block is not superblock
              //CANNOT GET FREE BLOCK ERROR
              return;
            }

            //write filled datablock to disk
            tempInodeBlock.blocks[currentDataBlock] = freeBlockNum; 
            bfs.write_block(freeBlockNum, &dataArr[v]); 


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
  dirblock_t currentDirectory;
  inode_t currentInode;
  datablock_t currentDataBlock;
  string outputMsg = "";
  int totNumBlocks;
  int leftOverBytes = 0;

 bfs.read_block(curr_dir, (void* ) &currentDirectory);
 for(int i = 0; i < currentDirectory.num_entries; i++)  {//iterates through block entries
    if(strcmp(currentDirectory.dir_entries[i].name, name) == 0)  {//checks if names match
      bfs.read_block(currentDirectory.dir_entries[i].block_num, &currentInode); //reads block to check meta data 
      if(currentInode.magic == INODE_MAGIC_NUM) {//entry is a inode
        //file target found
        totNumBlocks = (currentInode.size/BLOCK_SIZE) + 1;
        for(int z = 0; z < totNumBlocks - 1; z++) {
          bfs.read_block(currentInode.blocks[z], (void *) &currentDataBlock);
          for(int j=0; j < BLOCK_SIZE; j++) {
            outputMsg = outputMsg + currentDataBlock.data[j];
          }
        }

        leftOverBytes = (currentInode.size - (totNumBlocks - 1)*BLOCK_SIZE);
        //read final block seperately in case not full
        bfs.read_block(currentInode.blocks[totNumBlocks - 1], (void *) &currentDataBlock);
        for(int j=0; j < leftOverBytes; j++) {
          outputMsg = outputMsg + currentDataBlock.data[j];
        }

        //output Message contains the file data


      }
      else  {
        //NOT A DIRECTORY
        return;
      }
    }     
  }
  
  //NOT FOUND ERROR
  return;
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n)
{
  dirblock_t currentDirectory;
  inode_t currentInode;
  datablock_t currentDataBlock;
  int printSize;
  string outputMsg = "";
  int totNumBlocks;
  int leftOverBytes = 0;

 bfs.read_block(curr_dir, (void* ) &currentDirectory);
 for(int i = 0; i < currentDirectory.num_entries; i++)  {//iterates through block entries
    if(strcmp(currentDirectory.dir_entries[i].name, name) == 0)  {//checks if names match
      bfs.read_block(currentDirectory.dir_entries[i].block_num, &currentInode); //reads block to check meta data 
      if(currentInode.magic == INODE_MAGIC_NUM) {//entry is a inode
        if(n >= currentInode.size)  {
          //print everything in file
          printSize = currentInode.size;      
        }
        else  {
          //print n bytes
          printSize = n;
        }

        totNumBlocks = (printSize/BLOCK_SIZE) + 1;
        for(int z = 0; z < totNumBlocks - 1; z++) {
          bfs.read_block(currentInode.blocks[z], (void *) &currentDataBlock);
          for(int j=0; j < BLOCK_SIZE; j++) {
            outputMsg = outputMsg + currentDataBlock.data[j];
          }
        }
        leftOverBytes = (printSize - (totNumBlocks - 1)*BLOCK_SIZE);
        //read final block seperately in case not full
        bfs.read_block(currentInode.blocks[totNumBlocks - 1], (void *) &currentDataBlock);
        for(int j=0; j < leftOverBytes; j++) {
          outputMsg = outputMsg + currentDataBlock.data[j];
        }   

        //output message contains the file data

      }
      else  {
        //NOT A DIRECTORY
        return;
      }
    }     
  }
  
  //NOT FOUND ERROR
  return;
}

// delete a data file
void FileSys::rm(const char *name)
{
  //NOTE: ASSUMING THAT I CAN JUST GIVE BLOCK BACK AND NOT HAVE TO DELETE THE MEMORY
  dirblock_t currentDirectory;
  inode_t currentInode;
  int totalNumBlocks = 0;
  short inodeBlockNum = 0;

 bfs.read_block(curr_dir, (void* ) &currentDirectory);
 for(int i = 0; i < currentDirectory.num_entries; i++)  {//iterates through block entries
   if(strcmp(currentDirectory.dir_entries[i].name, name) == 0)  {//checks if names match
      bfs.read_block(currentDirectory.dir_entries[i].block_num, &currentInode); //reads block to check meta data 
      if(currentInode.magic == INODE_MAGIC_NUM) {//entry is a inode


        //update inode parent directory meta data
        inodeBlockNum = currentDirectory.dir_entries[i].block_num;
        for(int j = i; j < currentDirectory.num_entries; j++) { //shift down the entries in parent directory
          currentDirectory.dir_entries[j] = currentDirectory.dir_entries[j+1];
        }
        currentDirectory.num_entries--;
        bfs.write_block(curr_dir, (void*) &currentDirectory);

        totalNumBlocks = ceil(static_cast<double>(currentInode.size/BLOCK_SIZE));
        //reclaim Inode data blocks
        for(int z = 0; z < totalNumBlocks; z++) {
          bfs.reclaim_block(currentInode.blocks[z]);
        }
        //reclaim inode block
        bfs.reclaim_block(inodeBlockNum);

      }
      else  { 
       //ENTRY IS A DIRECTORY
       return;
      }
    }     
  }

  //NOT FOUND ERROR
  return;
  
}

// display stats about file or directory
void FileSys::stat(const char *name)
{
  dirblock_t currentDirectory;
  dirblock_t testDir;
  inode_t currentInode;


 bfs.read_block(curr_dir, (void* ) &currentDirectory);
 for(int i = 0; i < currentDirectory.num_entries; i++)  {//iterates through block entries
    if(strcmp(currentDirectory.dir_entries[i].name, name) == 0)  {//checks if names match
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void* )&testDir); //reads block to check meta data 
      if(testDir.magic == INODE_MAGIC_NUM) {//entry is a inode
        bfs.read_block(currentDirectory.dir_entries[i].block_num, (void* ) &currentInode);
        cout << "Inode block: " << currentDirectory.dir_entries[i].block_num << endl;
        cout << "Bytes in file: " << currentInode.size << endl;
        cout << "Number of blocks: " << (ceil(static_cast<double>(currentInode.size/BLOCK_SIZE) + 1)) << endl;
        cout << "First block: " << currentInode.blocks[0];
      }
      else  { //entry is a directory
        cout << "Directory name: " << currentDirectory.dir_entries[i].name << endl;
        cout << "Directory block: " << currentDirectory.dir_entries[i].block_num << endl;
      }
    }     
  }

  //NOT FOUND ERROR
  return;
}

// HELPER FUNCTIONS (optional)
