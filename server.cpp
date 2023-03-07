#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 1024

#include "FileSys.h"
using namespace std;

enum CommandType
{
    mkdir,
    ls,
    cd,
    home,
    rmdir_cmd, // Prevent name colision with an existing `rmdir` function.
    create,
    append,
    stat,
    cat,
    head,
    rm,
    quit,
    invalid
};

struct Command
{
    CommandType type;
    string file;      // cmd.name of the file to manipulate
    string data;      // data to add or for the append or number for head
};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Usage: ./nfsserver port#\n";
        return -1;
    }
    int port = atoi(argv[1]);

    // networking part: create the socket and accept the client connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        cout << "ERROR: Failed to open socket.\n";
        return -1;
    }

    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_address_len = sizeof(client_addr);

    bzero((char *)&server_addr, sizeof(server_addr));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror( "Error binding listening socket");
        close(sock);
        exit(1);
    }

    if (listen(sock, 10) < 0)
    {
        perror("Error listening for connections");
        close(sock);
        exit(1);
    }

    /*
      if (accept(listen_sock, (sockaddr*) &client_addr, &addr_len) < 0) {
        cout << "Error accepting connection" << endl;
        close(listen_sock);
        return;
      }
    */

    // mount the file system
    FileSys fs;
    fs.mount(sock); // assume that sock is the new socket created
                    // for a TCP connection between the client and the server.

    // loop: get the command from the client and invoke the file
    // system operation which returns the results or error messages back to the clinet
    // until the client closes the TCP connection.

    while (true)
    {
        // Accept a new connection
        int client_sock = accept(sock, (sockaddr *)&client_addr, &client_address_len);
        if (client_sock < 0)
        {
            perror("Error accepting connection");
            continue;
        }
        while (true)
        {
            // maxBufferSize
            char* buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            cout << "test" << endl;
            int x = read(sock, buffer, sizeof(buffer));
            if (x == -1 || x == 1)
            {
                std::cerr << strerror (errno);
                cout << "client disconnected" << endl;
                break;
            }
            Command command;
            CommandType type = command.type;
            const char*file = command.file.c_str();
            const char*data = command.data.c_str();

            if (type == cd)
            {
                fs.cd(file);
                continue;
            }
            else if (type == home)
            {
                fs.home();
                continue;
            }

            else if (type == mkdir)
            {
                fs.mkdir(file);
                continue;
            }

            else if (type == rmdir_cmd)
            {
                fs.rmdir(file);
                continue;
            }

            else if (type == ls)
            {
                fs.ls();
                continue;
            }

            else if (type == create)
            {
                fs.create(file);
                continue;
            }

            else if (type == append)
            {
                fs.append(file,data); 
                continue; 
            }

            else if (type == cat)
            {
                fs.cat(file);
                continue;
            }

            else if (type == head)
            {
                fs.head(file,stoi(data)); 
                continue;
            }

            else if (type == rm)
            {
                fs.rm(file);
                continue;
            }

            else if (type == stat)
            {
                fs.stat(file);
                continue;
            }
        }
    }

    // close the listening socket
    close(sock);

    // unmout the file system
    fs.unmount();
    return 0;
}
