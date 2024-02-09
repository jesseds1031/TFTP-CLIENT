#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <errno.h> // for retrieving the error number.
#include <netinet/in.h>
#include <signal.h> // for the signal handler registration.
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strerror function.
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include "../tftp/tftp.cpp"
#include <cmath>
#include <cstdint>


#define SERV_HOST_ADDR "10.158.82.34" // REPLACE WITH SERVER IP ADDRESS
#define SERV_UDP_PORT 51870
#define MAX_DATA_SIZE 512
#define DATA_OFFSET 4
#define TIMEOUT 3
#define MAX_TIMEOUTS 10
#define MODE "octet"
using namespace std;

const string RRQ = "-r";
const string WRQ = "-w";

//track timeouts
int timeoutCounter = 0;

// to handle timeouts
void handle_timeout(int signum)
{
    timeoutCounter++;
    printf("timeout occurred, resending packet...\n");
    cout << "timeout #: " << timeoutCounter << endl;
}

// to register timeout handler
int register_handler()
{
    //printf("running register_handle\n");
    int r_value = 0;
    // register handler function
    r_value = ((intptr_t) signal( SIGALRM, handle_timeout));
    //ensure register success
    if (r_value == ((intptr_t) SIG_ERR))
    {
        printf("Error registering functon handle_timeout");
        printf("signal() error: %s.\n", strerror(errno));
        return -1;
    }
    //disable the restart of system call
    r_value = siginterrupt(SIGALRM, 1);
    if (r_value == -1)
    {
        printf("invalid sig number. \n");
        return -1;
    }
    return 0;
}

// function to take care all of all parts of read requests
bool handleRRQ(int sockfd, TFTP &tftpClient, ofstream &file, char filename[], sockaddr_in serv_addr, socklen_t servlen)
{

    bool success = tftpClient.sendReqPacket("RRQ", sockfd, filename, serv_addr, servlen);
    if (!success)
    {
        cout << "Read Request Failed" << endl;
        exit(3);
    }

    unsigned short ackNum = 1;
    unsigned short dataNum = 0;

    for (;;)
    {
        sleep(1);

        //check if max timeouts occurred
        if (timeoutCounter >= MAX_TIMEOUTS)
        {
            printf("Max timeouts exceeded, closing program\n");
            // terminate connection
            exit(0);
        }


        bool rcvSuccess = tftpClient.recvPacket(sockfd, MAX_DATA_SIZE, serv_addr, servlen);

        if (tftpClient.buffer[1] == OP_CODE_ERROR)
        {
            string st(reinterpret_cast<char *>(tftpClient.buffer), sizeof(tftpClient.buffer));
            cout << "Recieved Error Packet: " << st << ". Exiting program." << endl;
            exit(0);
        }

        //timeout alarm
        cout << "setting a timeout alarm" << endl;
        int x = alarm(TIMEOUT);
        printf("Waiting to receive Data from server\n");

        if (!rcvSuccess)
        {
            if (errno == EINTR)
            {
                printf("timeout triggered!\n");
                continue;
            }
        }
        else 
        {
            printf("data received from server, clearing timeout alarm\n");
            alarm(0);
            //reset counter
            timeoutCounter = 0;
        }

        //check if max timeouts has occurred
        if (timeoutCounter >= MAX_TIMEOUTS)
        {
            // terminate connection
            exit(0);
        }

        if (tftpClient.buffer[1] != OP_CODE_DATA)
        {
            cout << "Packet is not Data tftpClient" << endl;
            exit(3);
        }

        dataNum++;
        cout << "ackNum: " << ackNum << endl;
        cout << "dataNum: " << dataNum << endl;
        if (dataNum != ackNum)
        {
            cout << "Data Block # != Ack Block #" << endl;
            exit(3);
        }

        string s(reinterpret_cast<char *>(tftpClient.buffer), sizeof(tftpClient.buffer));
        unsigned short lastbyte = tftpClient.buffer[MAX_DATA_SIZE - 1];
        // for (int i = 0; i < s.length(); i++)
        // {
        //     printf("\\x%.2x", s[i]);
        // }

        bool sendSuccess = tftpClient.sendAckPacket(ackNum, sockfd, serv_addr, servlen);

        if (rcvSuccess && sendSuccess)
        {
            cout << "Ack #: " << ackNum << " sent" << endl;
            // write to file
            tftpClient.writeToFile(file, s);
            // cout << "lastbyte: " << lastbyte << endl;
            if (lastbyte == '\0')
            {
                break;
            }
            ackNum++;
        }
    }
    return true;
}

// function to take care all of all parts of write requests
bool handleWRQ(int sockfd, TFTP &tftpClient, ifstream &inputFile, char filename[], sockaddr_in serv_addr, socklen_t servlen)
{
    bool success = tftpClient.sendReqPacket("WRQ", sockfd, filename, serv_addr, servlen);
    if (!success)
    {
        cout << "Write Request Failed" << endl;
        return false;
    }

    unsigned short ackNum = 0;
    unsigned short dataNum = 1;

    int i = 0;
    char file[MAX_DATA_SIZE];
    if (inputFile.is_open())
    {
        while (!inputFile.eof())
        {
            inputFile >> file[i];
            if (i == 511)
            {

                //check if max timeouts occurred
                if (timeoutCounter >= MAX_TIMEOUTS)
                {
                    printf("Max timeouts exceeded, closing program\n");
                    // terminate connection
                    exit(0);
                }

                //timeout alarm
                cout << "setting a timeout alarm" << endl;
                int x = alarm(TIMEOUT);
                printf("Waiting to receive Ack from server\n");


                // recive ack from the server
                bool recvSuccess = tftpClient.recvPacket(sockfd, MAX_DATA_SIZE, serv_addr, servlen);

                if (tftpClient.buffer[1] == OP_CODE_ERROR)
                {
                    string st(reinterpret_cast<char *>(tftpClient.buffer), sizeof(tftpClient.buffer));
                    cout << "Recieved Error Packet: " << st << ". Exiting program." << endl;
                    exit(0);
                }

                if (!recvSuccess)
                {
                    if (errno == EINTR)
                    {
                        printf("timeout triggered!\n");
                        continue;
                    }
                }
                else 
                {
                    printf("data received from server, clearing timeout alarm\n");
                    alarm(4);
                    //reset counter
                    timeoutCounter = 0;
                }

                //check if max timeouts occurred
                if (timeoutCounter >= MAX_TIMEOUTS)
                {
                    // terminate connection
                    exit(0);
                }

                if (tftpClient.buffer[1] != OP_CODE_ACK)
                {
                    cout << "Packet is not Ack tftpClient" << endl;
                    exit(3);
                }
                cout << "Ack #: " << ackNum << endl;
                // send data tftpClient to server
                bool sendSuccess = tftpClient.sendDataPacket(sockfd, file, filename, dataNum, serv_addr, servlen);
                if (!sendSuccess)
                {
                    cerr << "Cannot Send Data Packet: " << dataNum << endl;
                    exit(2);
                }
                cout << "Data Packet #: " << dataNum << endl;
                i = 0;
                bzero(file, sizeof(file));

                if (dataNum != (ackNum + 1))
                {
                    cout << "Data Block # and Ack Block # are not consistent" << endl;
                    exit(3);
                }
                ackNum++;
                dataNum++;
            }
            else
            {
                i++;
            }
        }
        // send the last tftpClient
        // cout << "AckNum last: " << ackNum << endl;
        // cout << "dataNum last: " << dataNum << endl;
        // recive ack from server
        bool recvSuccess = tftpClient.recvPacket(sockfd, MAX_DATA_SIZE, serv_addr, servlen);
       
        if (!recvSuccess)
        {
            cerr << "Cannot Recieve Ack Packet: " << ackNum << endl;
            exit(1);
        }
        if (tftpClient.buffer[1] != OP_CODE_ACK)
        {
            cout << "Packet is not Ack tftpClient" << endl;
            exit(3);
        }
        cout << "Ack #: " << ackNum << endl;
        bool sendSuccess = tftpClient.sendDataPacket(sockfd, file, filename, dataNum, serv_addr, servlen);
        if (!sendSuccess)
        {
            cerr << "Cannot Send Data Packet: " << dataNum << endl;
            exit(2);
        }
        cout << "Data Packet #: " << dataNum << endl;
        recvSuccess = tftpClient.recvPacket(sockfd, MAX_DATA_SIZE, serv_addr, servlen);
       
        if (!recvSuccess)
        {
            cerr << "Cannot Recieve Ack Packet: " << ackNum << endl;
            exit(1);
        }
        if (tftpClient.buffer[1] != OP_CODE_ACK)
        {
            cout << "Packet is not Ack tftpClient" << endl;
            exit(3);
        }
        ackNum++;
        cout << "Ack #: " << ackNum << endl;
        i = 0;
        bzero(file, sizeof(file));
        inputFile.close();
        return true;
    }
    return true;
}

// main client logic
int main(int argc, char *argv[])
{
    TFTP tftpClient = TFTP();
    /* We need to set up two addresses, one for the client and one for */
    /* the server.                                                     */
    struct sockaddr_in cli_addr, serv_addr;
    int sockfd;
    cout << "argc: " << argc << endl;

    if (argc == 5)
    {
        int newUDPPort = atoi(argv[4]);
        sockfd = tftpClient.createSocket(sockfd, serv_addr, cli_addr, newUDPPort, "client");
        cout << "Using Port #: " << newUDPPort << " and Sockfd: " << sockfd << endl;
    }
    else
    {
        sockfd = tftpClient.createSocket(sockfd, serv_addr, cli_addr, SERV_UDP_PORT, "client");
        cout << "Using Port #: " << SERV_UDP_PORT << " and Sockfd: " << sockfd << endl;
    }

    if (argc < 3)
    {
        cerr << "please rerun with the request type and filename" << endl;
        exit(3);
    }

    string request = argv[1];
    string file = argv[2];
    cout << "request:" << request << endl;
    cout << "file: " << file << endl;

    char inputFileName[file.length()];
    strcpy(inputFileName, file.c_str());
    socklen_t servlen = sizeof(struct sockaddr);

    if (register_handler() != 0)
    {
        printf("Could not register timeout\n");
    }

    ifstream inputFile;

    // Read request
    if (request == RRQ)
    {

        // check if file already exists
        
        inputFile.open(inputFileName);
        if (inputFile.is_open())
        {
            // file already exists

            cout << "error: File already exists, do you wish to overwrite: " << inputFileName 
                << "? enter y for yes, n for no." << endl;
            
            char temp;
            cin >> temp;

            if (temp == 'n')
            {
                cout << "exiting now." << endl;
                exit(0);
            }
            inputFile.close();

        }
        else
        {
            inputFile.close();
        }

        
        ofstream file;
        file << noskipws;
        file.open(inputFileName);
        if (!file)
        {
            cout << "Cannot open file" << endl;
        }

        if (handleRRQ(sockfd, tftpClient, file, inputFileName, serv_addr, servlen))
        {
            file.close();
        }
        else
        {
            cout << "Read Request Failed" << endl;
            exit(3);
        }
    }
    else if (request == WRQ)
    {
         // check permissions
        if (access(inputFileName, R_OK) != 0)
        {
            cout << "error: access violation, cannot access: " << inputFileName << ". Exiting." << endl;
            exit(0);
        }

        inputFile.open(inputFileName);
        if (!inputFile.is_open())
        {
            // file not found
            cout << "error: file not found error. Exiting." << endl;
            exit(0);
        }

        ifstream inputFile;
        inputFile >> noskipws;
        inputFile.open(inputFileName);
        // * if client want to write non-existing file to server, handle the error
        // * print error message on the client side
        if (!inputFile.is_open())
        {
            cerr << "File: " << inputFileName << " Not Found" << endl;
            exit(0);
        }
        if (handleWRQ(sockfd, tftpClient, inputFile, inputFileName, serv_addr, servlen))
        {
            inputFile.close();
        }
        else
        {
            cout << "Write Request Failed" << endl;
            exit(3);
        }
    }
    return 0;
}
