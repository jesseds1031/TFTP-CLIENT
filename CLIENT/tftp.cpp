#include "../tftp/tftp.h"

tftp::tftp()
{
    len = 0;
}

void tftp::clearPacket()
{
    bzero(buffer, sizeof(buffer));
    len = MAX_DATA_SIZE;
}

void tftp::createACK(unsigned short blkNum)
{
    clearPacket();
    unsigned short *opCodePtr = (unsigned short *)buffer;
    *opCodePtr = htons(OP_CODE_ACK);
    opCodePtr++;
    unsigned short *blockNum = opCodePtr;
    *blockNum = htons(blkNum);
}

void tftp::createReqPacket(string requestType, char *fileName)
{
    unsigned short opcode;
    char *mode = "Octet";
    // char buffer[MAX_DATA_SIZE];
    clearPacket();

    if (requestType == "RRQ")
    {
        opcode = OP_CODE_RRQ;
    }
    else if (requestType == "WRQ")
    {
        opcode = OP_CODE_WRQ;
    }
    // first 2 bytes is Opcode
    unsigned short *opCodePtr = (unsigned short *)buffer;
    // cout << opCodePtr << endl;
    *opCodePtr = htons(opcode);
    opCodePtr++;
    // cout << opCodePtr << endl;
    // file name field start after the opcode (from 3rd bytes)
    char *fileNameData = (char *)opCodePtr;
    // cout << strlen(fileName) << endl;
    strncpy(fileNameData, fileName, strlen(fileName));
    // write 0 to next byte
    char *nullTerminate = fileNameData + strlen(fileName);
    *nullTerminate = htons(NULL_TERMINATE);
    nullTerminate++;
    // write mode
    char *modeStr = nullTerminate;
    strncpy(modeStr, mode, strlen(mode));
    nullTerminate = modeStr + strlen(mode);

    // write last 0 to the packet
    *nullTerminate = htons(NULL_TERMINATE);
}

void tftp::createErrorPacket(unsigned int errCode)
{
    // for error message
    const char *errMsg;
    // set up errMsg based on code
    switch (errCode)
    {
    case 1:
        errMsg = "File Not Found";
        break;
    case 2:
        errMsg = "Access Violation for this file";
        break;
    case 6:
        errMsg = "File already exists";
        break;
    default:
        errMsg = "Unknown error";
    }
    unsigned short opcode = OP_CODE_ERROR;

    clearPacket();

    // fill out packet
    unsigned short *opCodePtr = (unsigned short *)buffer;

    *opCodePtr = htons(opcode);

    opCodePtr++;
    // write errCode
    unsigned short *error = opCodePtr;
    *error = htons(errCode);
    *opCodePtr++;

    // write errMsg
    char *errMsgData = (char *)opCodePtr;

    strncpy(errMsgData, errMsg, strlen(errMsg));

    // write 0
    char *nullTerminate = errMsgData + strlen(errMsg);
    *nullTerminate = htons(NULL_TERMINATE);
    // write mode
}

void tftp::createDataPacket(char *file, string fileName, unsigned short blockNum)
{

    clearPacket();

    unsigned short *opCodePtr = (unsigned short *)buffer;

    // add opcode
    *opCodePtr = htons(OP_CODE_DATA);
    *opCodePtr++;

    // add block num

    unsigned short *blockNumPtr = opCodePtr;

    *blockNumPtr = htons(blockNum);

    cout << "file" << endl;

    for (int i = 0; i < sizeof(file); i++)
    {
        printf("x0%X,", file[i]);
    }

    char *fileData = buffer + DATA_OFFSET;

    strncpy(fileData, file, strlen(file));
    cout << "buffer" << endl;

    for (int i = 0; i < sizeof(buffer); i++)
    {
        printf("x0%X,", buffer[i]);
    }
}

string tftp::getFileName()
{
    int counter;
    string s(reinterpret_cast<char *>(buffer), sizeof(buffer));
    string newString = "";
    for (int i = 2; i < s.length(); i++)
    {
        if (s[i] == 'o' || s[i] == 'O')
        {
            break;
        }
        else
        {
            newString += s[i];
        }
    }

    cout << "fileName: " << newString << endl;
    return newString;
}

void tftp::writeFile(string fileName, string packets)
{
    ofstream file;
    file.open(fileName);

    if (!file)
    {
        cout << "Cannot open file" << endl;
    }
    for (int j = 0; j < packets.length(); j++)
    {
        char c = packets[j];
        if (c == '?' || c == '@')
        {
            break;
        }
        if (isprint(c))
        {
            file << packets[j];
        }
    }

    file.close();
}