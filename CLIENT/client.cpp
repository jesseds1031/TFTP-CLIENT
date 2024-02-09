#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <errno.h> // for retrieving the error number.
#include <netinet/in.h>
#include <signal.h> // for the signal handler registration.
#include <stdio.h>
#include <cstdio>
#include <stdlib.h>
#include <string.h> // for strerror function.
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include "../tftp/tftp.h"

#define SERV_HOST_ADDR "10.158.82.34" // REPLACE WITH SERVER IP ADDRESS
#define SERV_UDP_PORT 51851
#define MAX_DATA_SIZE 512
#define DATA_OFFSET 4
#define MODE "octet"
using namespace std;

const string RRQ = "-r";
const string WRQ = "-w";


// void sendPacket(struct sockaddr_in &pserv_addr, int sockfd, int len)
// {
//     // ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
//     // k const struct sockaddr *dest_addr, socklen_t addrlen);

//     socklen_t servlen = sizeof(pserv_addr);
//     // modify this instead, i copied from prof's code
//     sendto(sockfd, buffer, len, 0, reinterpret_cast<sockaddr *>(&pserv_addr), servlen);
// }

// void run(string request, const char file[])
// {
//     if (request == RRQ)
//     {
//         // send out read request
//         // createRRQ("file.txt");
//         // recieve data block #1
//         int len = 3 + strlen(file) + request.size();
//     }
//     else if (request == WRQ)
//     {
//         // send out write request

//         // recieve ack #1
//         // send out data block #1
//         int len = 3 + strlen(file) + request.size();
//     }
//     else
//     {
//         cerr << "invalid command, please try again" << endl;
//     }
// }



int main(int argc, char *argv[])
{
    tftp packet = tftp();
    int sockfd;

    // for commandline port option
    bool isDefaultPort = true;
    int newUDPPort;

    if (argc == 5)
    {
        newUDPPort = atoi(argv[4]);
        isDefaultPort = false;
    }

    /* We need to set up two addresses, one for the client and one for */
    /* the server.                                                     */

    struct sockaddr_in cli_addr, serv_addr;

    /* Initialize first the server's data with the well-known numbers. */

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    /* The system needs a 32 bit integer as an Internet address, so we */
    /* use inet_addr to convert the dotted decimal notation to it.     */

    serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    
    // new port option
    if (!isDefaultPort)
    {
        serv_addr.sin_port = htons(newUDPPort);
        cout << "Using Port #: " << newUDPPort << endl;
    }
    else
    {
        serv_addr.sin_port = htons(SERV_UDP_PORT);
        cout << "Using Default Port #: " << SERV_UDP_PORT << endl;
    }

    /* Create the socket for the client side.                          */

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        cout << "Cannot create socket" << endl;
        exit(1);
    }

    /* Initialize the structure holding the local address data to      */
    /* bind to the socket.                                             */

    bzero((char *)&cli_addr, sizeof(cli_addr));
    cli_addr.sin_family = AF_INET;

    /* Let the system choose one of its addresses for you. You can     */
    /* use a fixed address as for the server.                          */

    cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* The client can also choose any port for itself (it is not       */
    /* well-known). Using 0 here lets the system allocate any free     */
    /* port to our program.                                            */

    cli_addr.sin_port = htons(0);

    /* The initialized address structure can be now associated with    */
    /* the socket we created. Note that we use a different parameter   */
    /* to exit() for each type of error, so that shell scripts calling */
    /* this program can find out how it was terminated. The number is  */
    /* up to you, the only convention is that 0 means success.         */
    int socketBind = bind(sockfd, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
    if (socketBind < 0)
    {
        cout << "Cannot create socket" << endl;
        exit(2);
    }

    if (argc < 3)
    {
        cerr << "please rerun with the request type and filename" << endl;
        exit(3);
    }

    cout << "Socket is working with: " << sockfd << endl;

    string request = argv[1];
    cout << "request:" << request << endl;
    string file = argv[2];
    cout << "file:" << file << endl;
    char inputFileName[file.length()];
    strcpy(inputFileName, file.c_str());

    ifstream inputFile;

    if (request == RRQ)
    {
        
        // check if file already exists
        
        inputFile.open(inputFileName);
        if (inputFile.is_open())
        {
            // file not found
            packet.createErrorPacket(6);
            string temp(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));

            cout << "error: "<< temp << endl;
            
            exit(0);
        }
        else
        {
            inputFile.close();
        }


        packet.createReqPacket("RRQ", inputFileName);
        // send out request for read
        int size = sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (size != sizeof(packet.buffer))
        {
            cout << "Cannot send" << endl;
            exit(3);
        }

        int n;
        packet.clearPacket();

        for (;;)
        {
            socklen_t servlen = sizeof(struct sockaddr);
            /* Receive data on socket sockfd, up to a maximum of MAXMESG       */
            n = recvfrom(sockfd, packet.buffer, MAX_DATA_SIZE, 0, (sockaddr *)&serv_addr, &servlen);
            
            string st(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));
            if (packet.buffer[1] == 5)
            {
                cout << "error packet received: "<< st << endl;
            }
            
            //cout << "n: " << n << endl;

            if (n != sizeof(packet.buffer))
            {
                cout << "Do not recieve" << endl;
            }
            string s(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));
            cout << s << endl;

            //packet.writeFile(inputFileName);
            if (n < 0)
            {
                cout << "Recieve Error" << endl;
            }
            
        }
    }
    else if (request == WRQ)
    {

        // check permissions
        if (access(inputFileName, R_OK) != 0)
        {
            packet.createErrorPacket(2);
            string temp(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));

            cout << "error: "<< temp << endl;
            
            exit(0);
        }

        inputFile.open(inputFileName);
        if (!inputFile.is_open())
        {
            // file not found
            packet.createErrorPacket(1);
            string temp(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));
            cout << "error: "<< temp << endl;
            exit(0);
        }

        //do we have permission?

        
        packet.createReqPacket("WRQ", inputFileName);
        // send out request for read
        int size = sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (size != sizeof(packet.buffer))
        {
            cout << "Cannot send" << endl;
            exit(3);
        }
        int n;
        packet.clearPacket();

        for (;;)
        {
            socklen_t servlen = sizeof(struct sockaddr);
            /* Receive data on socket sockfd, up to a maximum of MAXMESG       */
            n = recvfrom(sockfd, packet.buffer, MAX_DATA_SIZE, 0, (sockaddr *)&serv_addr, &servlen);
            string s(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));
            if (packet.buffer[1] == 5)
            {
                cout << "error packet received: "<< s << endl;
            }
            
            cout << "n: " << n << endl;

            if (n != sizeof(packet.buffer))
            {
                cout << "Did not recieve" << endl;
            }

            if (packet.buffer[1] == 4)
            {
                // count blk num
                unsigned short blkNumCounter = 0;
 
                int i = 0;

                inputFile >> noskipws;
                // hold current file data
                char file[MAX_DATA_SIZE];

                if (inputFile.is_open())
                {
                    int counter = 0;
                    int packetCounter = 0;
                    while (!inputFile.eof())
                    {
                        
                        if (counter == 508)
                        {
                            cout << "hello: " << counter << endl;
                            packet.createDataPacket(file, inputFileName, blkNumCounter);

                            blkNumCounter++;
                            
                            int size = sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                            packet.clearPacket();
                            bzero(file, sizeof(file));
                            if (size != sizeof(packet.buffer))
                            {
                                cout << "Cannot send" << endl;
                                exit(3);
                            }
                            //reset counter for new packet
                            counter = 0;
                            packetCounter++;
                        }
                        else
                        {
                            inputFile >> file[i];
                            i++;
                            counter++;
                        }
                        
                    }
                    cout << "packets: " << packetCounter << endl;
                    inputFile.close();

                    if (file[0] != 0)
                    {

                        packet.createDataPacket(file, inputFileName, blkNumCounter);

                        blkNumCounter++;

                        // for (int i = 0; i < sizeof(packet.buffer); i++)
                        // {
                        //     printf("x0%X,", packet.buffer[i]);
                        // }

                        cout << "here" << endl;
                        
                        int size = sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                        
                        if (size != sizeof(packet.buffer))
                        {
                            cout << "Cannot send" << endl;
                            exit(3);
                        }
                        packet.clearPacket();
                        bzero(file, sizeof(file));
                    }

                    packet.clearPacket();
                    int size = sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                        
                }
                else
                {
                    cerr << "error reading file" << endl;
                    exit(0);
                }
                    
            }
            
            if (n < 0)
            {
                cout << "Recieve Error" << endl;
            }
            
        }

    }

    // int n;
    // char mesg[MAX_DATA_SIZE];
    // for (;;)
    // {
    //     int size = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    //     if (size != sizeof(buffer))
    //     {
    //         cout << "Cannot send" << endl;
    //         exit(3);
    //     }
    //     socklen_t servlen = sizeof(struct sockaddr);
    //     /* Receive data on socket sockfd, up to a maximum of MAXMESG       */
    //     n = recvfrom(sockfd, mesg, MAX_DATA_SIZE, 0, (sockaddr *)&serv_addr, &servlen);
    //     cout << "n: " << n << endl;

    //     if (n != sizeof(mesg))
    //     {
    //         cout << "Do not recieve" << endl;
    //     }
    //     string s(reinterpret_cast<char *>(mesg), sizeof(mesg));
    //     cout << s << endl;

    //     if (n < 0)
    //     {
    //         cout << "Recieve Error" << endl;
    //     }
    // }

    // string s(reinterpret_cast<char *>(buffer), sizeof(buffer));
    // cout << s << endl;
    return 0;
}

// ssh cssmpi1h