// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <cmath>
#include <string>
using namespace std;

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <string.h>

string data;

// mounts the file system
void FileSys::mount(int sock)
{
  bfs.mount();
  curr_dir = 1;   // by default current directory is home directory, in disk block #1
  fs_sock = sock; // use this socket to receive file system operations from the client and send back response messages
  // msg(200, "");
}

// unmounts the file system
void FileSys::unmount()
{
  bfs.unmount();
  close(fs_sock);
  msg(200, "");
}

// make a directory
void FileSys::mkdir(const char *name)
{
  dirblock_t currentDirectoryBlock;
  bfs.read_block(curr_dir, (void *)&currentDirectoryBlock);

  // check if name exceeds limit
  if (strlen(name) > MAX_FNAME_SIZE)
  {
    msg(504, "");
    return;
  }
  // check if max directory entries exceeded
  if (currentDirectoryBlock.num_entries >= MAX_DIR_ENTRIES)
  {
    msg(506, "");
    return;
  }
  // check if name already exists - linear search
  for (unsigned int i = 0; i < currentDirectoryBlock.num_entries; i++)
  {
    if (strcmp(currentDirectoryBlock.dir_entries[i].name, name) == 0)
    { // checks if dir already exists
      msg(502, "");
      return;
    }
  }

  short blockID = bfs.get_free_block();
  if (blockID <= 0)
  { // check that given block is a valid block
    msg(505, "");
    return;
  }

  // setup new directory object
  struct dirblock_t newDir;

  // set new directory metadata
  newDir.magic = DIR_MAGIC_NUM;
  newDir.num_entries = 0;
  for (int j = 0; j < MAX_DIR_ENTRIES; j++)
  { // initialize all dir entries to empty
    newDir.dir_entries[j].block_num = 0;
  }

  // update parent directory meta data
  currentDirectoryBlock.dir_entries[currentDirectoryBlock.num_entries].block_num = blockID;
  strcpy(currentDirectoryBlock.dir_entries[currentDirectoryBlock.num_entries].name, name);
  currentDirectoryBlock.num_entries++;

  // write blocks that were changed back to disk (write child first then parent)
  bfs.write_block(blockID, (void *)&newDir);
  bfs.write_block(curr_dir, (void *)&currentDirectoryBlock);
  msg(200, "");
  return;
}

// switch to a directory
void FileSys::cd(const char *name)
{
  dirblock_t currentDirectory;
  dirblock_t targetDirectory;
  bfs.read_block(curr_dir, (void *)&currentDirectory); // read in current Directory

  for (unsigned int i = 0; i < currentDirectory.num_entries; i++)
  { // scan through entries in current directory
    if (strcmp(currentDirectory.dir_entries[i].name, name) == 0)
    {                                                                                      // checks if target directory exists in current directory
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&targetDirectory); // loads block to check block type
      if (targetDirectory.magic == DIR_MAGIC_NUM)
      {                                                       // checks if block loaded is a Directory block
        curr_dir = currentDirectory.dir_entries[i].block_num; // sets current directory to new block
        msg(200, "");
        return;
      }
      else
      { // block is Inode
        msg(500, "");
        return;
      }
    }
  }

  msg(503, "");
  return;
}

// switch to home directory
void FileSys::home()
{
  curr_dir = 1; // set current directory block number to home directory block
  msg(200, "");
  return;
}

// remove a directory
void FileSys::rmdir(const char *name)
{
  dirblock_t currentDirectory;
  dirblock_t cdDir;
  bfs.read_block(curr_dir, (void *)&currentDirectory);

  for (unsigned int i = 0; i < currentDirectory.num_entries; i++)
  { // scan through entries in current directory
    if (strcmp(currentDirectory.dir_entries[i].name, name) == 0)
    {                                                                            // checks if target directory exists in current directory
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&cdDir); // loads block to check block type
      if ((cdDir.magic == DIR_MAGIC_NUM))
      { // checks if block loaded is a Directory block
        // found target directory
        if (cdDir.num_entries == 0)
        { // ensure that directory is empty

          // update parent directory metadata
          currentDirectory.num_entries--;
          for (unsigned int x = i; x <= currentDirectory.num_entries; x++)
          { // shift down entries to fill gap
            currentDirectory.dir_entries[x] = currentDirectory.dir_entries[x + 1];
          }

          // write block with changes to disk (update parent first)
          bfs.write_block(curr_dir, (void *)&currentDirectory);

          // give back disk block (update child second)
          bfs.reclaim_block(currentDirectory.dir_entries[i].block_num);
          msg(200, "");
          return;
        }
        else
        {
          msg(507, "");
          return;
        }
      }
      else
      { // file is an inode
        msg(500, "");
        return;
      }
    }
  }

  msg(503, "");
  return;
}

// list the contents of current directory
void FileSys::ls()
{
  dirblock_t currentDirectory;
  dirblock_t tempBlock;
  string respMsg = "";

  bfs.read_block(curr_dir, (void *)&currentDirectory); // read current directory
  if (currentDirectory.num_entries == 0)
  {
    respMsg = "Empty Folder";
    msg(200, respMsg);
    return;
  }
  for (unsigned int i = 0; i < currentDirectory.num_entries; i++)
  {                                                                                // iterates through block entries
    bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&tempBlock); // reads block to check meta data

    if (tempBlock.magic == DIR_MAGIC_NUM)
    { // entry is a directory
      respMsg += (currentDirectory.dir_entries[i].name);
      respMsg += "/ ";
    }
    else if (tempBlock.magic == INODE_MAGIC_NUM)
    { // entry is a inode
      respMsg += (currentDirectory.dir_entries[i].name);
      respMsg += " ";
    }
    else
    {
      return;
    }
  }
  msg(200, respMsg);
  return;
}

// create an empty data file
void FileSys::create(const char *name)
{
  dirblock_t currBlock;

  bfs.read_block(curr_dir, (void *)&currBlock);

  // check if name exceeds limit
  if (strlen(name) > MAX_FNAME_SIZE)
  {
    msg(504, "");
    return;
  }

  // check if name already exists - linear search
  for (unsigned int i = 0; i < currBlock.num_entries; i++)
  {
    if (strcmp(currBlock.dir_entries[i].name, name) == 0)
    { // checks if dir already exists
      msg(502, "");
      return;
    }
  }

  // check if max directory entries exceeded
  if (currBlock.num_entries >= MAX_DIR_ENTRIES)
  {
    msg(506, "");
    return;
  }
  // all precondtions passed, now make inode for file

  // get new block to hold inode in current directory
  short newBlock = bfs.get_free_block();
  if (newBlock <= 0)
  { // check that given block is not superblock
    msg(505, "");
    return;
  }

  // create inode and setup meta data
  struct inode_t newFileInode;
  newFileInode.magic = INODE_MAGIC_NUM;
  newFileInode.size = 0;
  for (int z = 0; z < MAX_DATA_BLOCKS; z++)
  {
    newFileInode.blocks[z] = 0;
  }

  // update parent meta data

  currBlock.dir_entries[currBlock.num_entries].block_num = newBlock;
  strcpy(currBlock.dir_entries[currBlock.num_entries].name, name);
  currBlock.num_entries++;

  // write changed blocks to disk
  bfs.write_block(curr_dir, (void *)&currBlock);
  bfs.write_block(newBlock, (void *)&newFileInode);
  msg(200, "");
  return;
}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{
  int appDataSize = strlen(data);
  if (appDataSize == 0)
  { // no point in continuing
    return;
  }

  dirblock_t currBlock;
  inode_t tempInodeBlock;
  datablock_t tempDataBlock;

  int totalNewBlocks = 0;
  int currentDataBlock = 0;
  int currentDataBytesUsed = 0;
  int totNeededBlocks = 0;
  int dataBookmark = 0;
  int freeBlockNum = 0;
  bool isClean = false;
  bfs.read_block(curr_dir, (void *)&currBlock); // read current directory

  for (unsigned int i = 0; i < currBlock.num_entries; i++)
  { // iterates through block entries
    if (strcmp(currBlock.dir_entries[i].name, name) == 0)
    {                                                                              // checks if names match
      bfs.read_block(currBlock.dir_entries[i].block_num, (void *)&tempInodeBlock); // reads block to check meta data
      if (tempInodeBlock.magic == INODE_MAGIC_NUM)
      { // entry is a inode
        // file target found
        if ((tempInodeBlock.size + appDataSize) <= MAX_FILE_SIZE)
        { // check if append would execeed MAX_FILE_SIZE
          // operation is allowed

          // find next block to write to number of used blocks
          if (tempInodeBlock.size == 0)
          { // no data in file
            currentDataBlock = 0;
          }
          else
          {
            currentDataBlock = ceil(static_cast<double>(tempInodeBlock.size) / BLOCK_SIZE);
          }
          currentDataBytesUsed = (tempInodeBlock.size % BLOCK_SIZE);                                   // find if there is/amount of used bytes in last data block
          totNeededBlocks = ceil(static_cast<double>(appDataSize + tempInodeBlock.size) / BLOCK_SIZE); // total blocks needed to handle append data

          if (currentDataBytesUsed != 0)
          {                                                                                      // if there is already partially filled blocks of data, maybe need to be filled
            bfs.read_block(tempInodeBlock.blocks[currentDataBlock - 1], (void *)&tempDataBlock); // get partially filled data block
            for (int z = currentDataBytesUsed; !((z >= BLOCK_SIZE) || (data[dataBookmark] == (appDataSize))); z++)
            { // fill up last blocks empty space
              tempDataBlock.data[z] = data[dataBookmark];
              dataBookmark++;
            }

            // write updated block to memory
            bfs.write_block(tempInodeBlock.blocks[currentDataBlock - 1], (void *)&tempDataBlock);
            // how many new whole blocks needed to add rest of data

            totalNewBlocks = totNeededBlocks - currentDataBlock;
            currentDataBlock++;
          }
          else
          { // fresh append, with prev blocks already full of stored data
            isClean = true;
            totalNewBlocks = ceil(static_cast<double>(appDataSize) / BLOCK_SIZE);
          }
          // code to add whole new data blocks to append data to
          datablock_t *dataArr = new datablock_t[totalNewBlocks];
          for (int v = 0; v < totalNewBlocks; v++)
          {
            for (int t = 0; !((t >= BLOCK_SIZE) || (dataBookmark == (appDataSize))); t++)
            { // store data into datablock
              dataArr[v].data[t] = data[dataBookmark];
              dataBookmark++; // point to next char in data
            }

            freeBlockNum = bfs.get_free_block();
            if (freeBlockNum <= 0)
            { // check that given block is not superblock
              msg(505, "");
              return;
            }

            if (isClean && currentDataBlock == 0)
            {
              currentDataBlock++;
            }
            // write filled datablock to disk
            tempInodeBlock.blocks[currentDataBlock - 1] = freeBlockNum;
            bfs.write_block(freeBlockNum, (void *)&dataArr[v]);
            currentDataBlock++; // update what block is needed to be updated next
          }

          // update inode metadata
          tempInodeBlock.size = (tempInodeBlock.size + appDataSize);
          bfs.write_block(currBlock.dir_entries[i].block_num, &tempInodeBlock);
          delete[] dataArr;
          msg(200, "");
          return;
        }
        else
        {
          msg(508, "");
          return;
        }
      }
      else
      {
        msg(501, "");
        return;
      }
    }
  }

  msg(503, "");
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

  bfs.read_block(curr_dir, (void *)&currentDirectory);
  for (unsigned int i = 0; i < currentDirectory.num_entries; i++)
  { // iterates through block entries
    if (strcmp(currentDirectory.dir_entries[i].name, name) == 0)
    {                                                                                   // checks if names match
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&currentInode); // reads block to check meta data
      if (currentInode.magic == INODE_MAGIC_NUM)
      { // entry is a inode
        // file target found
        if (currentInode.size == 0)
        {
          return; // no point in continuing
        }
        totNumBlocks = (currentInode.size / BLOCK_SIZE) + 1;
        for (int z = 0; z < totNumBlocks - 1; z++)
        {
          bfs.read_block(currentInode.blocks[z], (void *)&currentDataBlock);
          for (int j = 0; j < BLOCK_SIZE; j++)
          {
            outputMsg += currentDataBlock.data[j]; // append each char to output data
          }
        }

        leftOverBytes = (currentInode.size - (totNumBlocks - 1) * BLOCK_SIZE);
        // read final block seperately in case not full
        bfs.read_block(currentInode.blocks[totNumBlocks - 1], (void *)&currentDataBlock);
        for (int j = 0; j < leftOverBytes; j++)
        {
          outputMsg += currentDataBlock.data[j];
        }

        // output Message contains the file data
        msg(200, outputMsg);
        return;
      }
      else
      {
        msg(501, "");
        return;
      }
    }
  }

  msg(503, "");
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

  bfs.read_block(curr_dir, (void *)&currentDirectory);
  for (unsigned int i = 0; i < currentDirectory.num_entries; i++)
  { // iterates through block entries
    if (strcmp(currentDirectory.dir_entries[i].name, name) == 0)
    {                                                                                   // checks if names match
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&currentInode); // reads block to check meta data
      if (currentInode.magic == INODE_MAGIC_NUM)
      { // entry is a inode
        if (n >= currentInode.size)
        {
          // print everything in file
          printSize = currentInode.size;
        }
        else
        {
          // print n bytes
          printSize = n;
        }

        totNumBlocks = (printSize / BLOCK_SIZE) + 1;
        for (int z = 0; z < totNumBlocks - 1; z++)
        {
          bfs.read_block(currentInode.blocks[z], (void *)&currentDataBlock);
          for (int j = 0; j < BLOCK_SIZE; j++)
          {
            outputMsg += currentDataBlock.data[j];
          }
        }
        leftOverBytes = (printSize - (totNumBlocks - 1) * BLOCK_SIZE);
        // read final block seperately in case not full
        bfs.read_block(currentInode.blocks[totNumBlocks - 1], (void *)&currentDataBlock);
        for (int j = 0; j < leftOverBytes; j++)
        {
          outputMsg += currentDataBlock.data[j];
        }

        // output message contains the file data
        msg(200, outputMsg);
        return;
      }
      else
      {
        msg(501, "");
        return;
      }
    }
  }

  msg(503, "");
  return;
}

// delete a data file
void FileSys::rm(const char *name)
{
  // NOTE: ASSUMING THAT I CAN JUST GIVE BLOCK BACK AND NOT HAVE TO DELETE THE MEMORY
  dirblock_t currentDirectory;
  inode_t currentInode;
  int totalNumBlocks = 0;
  short inodeBlockNum = 0;

  bfs.read_block(curr_dir, (void *)&currentDirectory);
  for (unsigned int i = 0; i < currentDirectory.num_entries; i++)
  { // iterates through block entries
    if (strcmp(currentDirectory.dir_entries[i].name, name) == 0)
    {                                                                                   // checks if names match
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&currentInode); // reads block to check meta data
      if (currentInode.magic == INODE_MAGIC_NUM)
      { // entry is a inode

        inodeBlockNum = currentDirectory.dir_entries[i].block_num;

        // update inode parent directory meta data
        for (unsigned int j = i; j < currentDirectory.num_entries; j++)
        { // shift down the entries in parent directory
          currentDirectory.dir_entries[j] = currentDirectory.dir_entries[j + 1];
        }
        currentDirectory.num_entries--;
        bfs.write_block(curr_dir, (void *)&currentDirectory);

        totalNumBlocks = ceil(static_cast<double>(currentInode.size / BLOCK_SIZE));
        // reclaim Inode data blocks
        for (int z = 0; z < totalNumBlocks; z++)
        {
          bfs.reclaim_block(currentInode.blocks[z]);
        }
        // reclaim inode block
        bfs.reclaim_block(inodeBlockNum);
        msg(200, "");
        return;
      }
      else
      {
        msg(501, "");
        return;
      }
    }
  }

  msg(503, "");
  return;
}

// display stats about file or directory
void FileSys::stat(const char *name)
{
  dirblock_t currentDirectory;
  dirblock_t testDir;
  inode_t currentInode;
  int temp;
  string respMsg = "";

  bfs.read_block(curr_dir, (void *)&currentDirectory);
  for (unsigned int i = 0; i < currentDirectory.num_entries; i++)
  { // iterates through block entries
    if (strcmp(currentDirectory.dir_entries[i].name, name) == 0)
    { // checks if names match
      // FOUND FILE
      bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&testDir); // reads block to check meta data
      if (testDir.magic == INODE_MAGIC_NUM)
      { // entry is a inode
        bfs.read_block(currentDirectory.dir_entries[i].block_num, (void *)&currentInode);
        respMsg += "Inode block: " + to_string(currentDirectory.dir_entries[i].block_num);
        respMsg += "\nBytes in file: " + to_string(currentInode.size);
        temp = (ceil(static_cast<double>(currentInode.size) / BLOCK_SIZE));
        respMsg += "\nNumber of blocks: " + to_string(temp);
        respMsg += "\nFirst block: ";
        if (temp > 0)
        { // non-empty data file
          respMsg += to_string(currentInode.blocks[0]);
        }
        else
        {
          respMsg += "0";
        }
      }
      else
      { // entry is a directory
        respMsg += "Directory name: ";
        respMsg += currentDirectory.dir_entries[i].name;
        respMsg += "/ ";
        respMsg += "\nDirectory block: ";
        respMsg += to_string(currentDirectory.dir_entries[i].block_num);
      }

      msg(200, respMsg);
      return;
    }
  }

  msg(503, "");
  return;
}
// HELPER FUNCTIONS (optional)
void FileSys::msg(int errorNum, string data)
{
  string errorCode = "";
  string outputMsg = "";
  switch (errorNum)
  {
  case 500:
    errorCode = "500 File is not a directory";
    break;
  case 501:
    errorCode = "501 File is a directory";
    break;
  case 502:
    errorCode = "502 File exits";
    break;
  case 503:
    errorCode = "503 File does not exist";
    break;
  case 504:
    errorCode = "504 File name is too long";
    break;
  case 505:
    errorCode = "505 Disk is full";
    break;
  case 506:
    errorCode = "506 Directory is full";
    break;
  case 507:
    errorCode = "507 Directory is not empty";
    break;
  case 508:
    errorCode = "508 Append exceeds maximum file size";
    break;
  case 200:
    errorCode = "200 OK";
    break;
  default:
    errorCode = "invalid Error Response";
  }

  string temp = "\r\n";

  outputMsg = errorCode + temp + "Length: " + to_string(data.length()) + temp + temp + data;
  cout << outputMsg << endl;

  for(int i = 0; i < outputMsg.length(); i++) {
    char p = outputMsg[i];
    int bytes_sent = 0;
    int x;
    while(bytes_sent < sizeof(char))  {
      x = send(this->fs_sock, (void*) &p, sizeof(char), 0);
      if(x == -1) {
        perror("write");
        close(this->fs_sock);
        exit(1);
      }
      bytes_sent += x;
    }
  }

}
