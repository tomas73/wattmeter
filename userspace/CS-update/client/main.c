/******************************************************************************/
/**
 * \file main.cpp
 *
 * \brief This is a TCP client that will fetch the contents of a file
 *        by connecting to a specific port, both provided
 *        as arguments to the program.
 *        The server transmits files using the following message format
 *
 *        | 4 bytes fileSize | fileSize number of bytes of contents |
 *
 *
 * \author Tomas Rosenkvist
 *
 * Copyright &copy; Maquet Critical Care AB, Sweden
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#define CHUNK_SIZE (1000)

/******************************************************************************/
/**
 *
 * Print a system error message on stdout, and exit with code 1
 *
 * \param msg The error message to be printed
 * \return  None
 *
 ******************************************************************************/
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/******************************************************************************/
/**
 *
 * Print usage information regarding this program
 *
 * \param none
 * \return none
 *
 ******************************************************************************/
static void usage(void)
{
    printf("file-trx-client server is a program to fetch files over a network\n");
    printf("a - IP address to connect to\n");
    printf("p - port to connect to\n");
    printf("f - File name to provide\n");
    printf("h - print this help\n");

    printf("\n");
}

/******************************************************************************/
/**
 *
 * Converts a string representation of a port number to an integer representation
 *
 * \param providedPort, pointer to string representing port
 * \return the integer representation of the provided port
 *
 ******************************************************************************/
int16_t parsePort(const char *providedPort)
{
    int16_t port;

    if (providedPort == NULL)
        {
            return -1;
        }

    port = atoi(providedPort);
    if (port == 0)
        {
            return -1;
        }
    return port;
}

/******************************************************************************/
/**
 *
 * Verifies that providedName represents a file that we have read access to
 *
 * \param providedName, the file name to verify
 * \return the verified file name, or NULL if not valid
 *
 ******************************************************************************/
char *parseFileName(const char *providedName)
{
    FILE *file = NULL;
    char *fileName;

    /* Would some plausabilitytesting be of use, i.e. can this do something bad */
    /* when provided with a bad file name ?                                     */

    if (providedName == NULL)
        {
            return NULL;
        }

    file = fopen(providedName, "w+");
    if (file != NULL)
        {
            fclose(file);
        }
    else
        {
            return NULL;
        }
    fileName = (char *)providedName;
    return fileName;
}

/******************************************************************************/
/**
 *
 * Verifies that providedIP is one of MON, BRE, PAN or localhost
 *
 * \param providedIp, the IP address to verify
 * \return the verified IP address, or NULL if invalid
 *
 ******************************************************************************/
char *parseServerIP(const char *providedIP)
{
    char *serverIP;

    serverIP = "1.1.1.66"; // BRE
    if (strcmp(providedIP, serverIP) == 0) return serverIP;

    serverIP = "1.1.1.77"; // MON
    if (strcmp(providedIP, serverIP) == 0) return serverIP;

    serverIP = "1.1.1.2"; // PAN
    if (strcmp(providedIP, serverIP) == 0) return serverIP;

    serverIP = "127.0.0.1"; // localhost
    if (strcmp(providedIP, serverIP) == 0) return serverIP;

    return NULL;
}


/******************************************************************************/
/**
 *
 * Reads a numnber of bytes from a file
 *
 * \param fd, the file descriptor identifying the file to read from
 * \param buf, pointer to a buffer to store read data in
 * \param numToRead, the number of bytes to read
 * \return the number of bytes read, or an error code if unsuccessful
 *
 ******************************************************************************/
int readXBytes(int fd, uint8_t *buf, int numToRead)
{
    int numRead = 0;
    int retval  = 0;

    while (numRead < numToRead)
        {
            retval = read(fd, buf + numRead, numToRead - numRead);
            if (retval > 0)
                {
                    numRead += retval;
                }
            else if (retval > 0)
                {
                    return -1;
                }
            else
                {
                    // EOF, WTF!
                    return -2;
                }
        }
    return numRead;
}


/******************************************************************************/
/**
 *
 * Entrypoint for file-trx-client
 *
 * A TCP socket is created and a cunnection is made to the specified port
 * and address.
 * Upon conection the following happens.
 * 1. The file size is read from the socket.
 * 2. size number of bytes is read from the socket and written to the
 *    file or to stdout
 * 4. File and socket is closed
 *
 *
 * \param -a server IP, IP address of server
 * \param -f file to store read data in, stdout if omitted
 * \param -p port to connect to
 * \param -h print help
 * \return 0 if successful, otherwise 1
 *
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int32_t opt, retval, numRead, numToRead, length;
    uint8_t buffer[CHUNK_SIZE];
    int sockfd;
    FILE *fp;
    struct sockaddr_in serv_addr;
    char *serverIP = NULL;
    char *fileName = NULL;
    int16_t port = -1;

    /* Parse command line for required information */
    while ((opt = getopt(argc, argv, "a:p:f:h")) != -1)
        {
            switch (opt)
                {
                case 'a':
                    {
                        serverIP = parseServerIP(optarg);
                        if (serverIP == NULL) error("Server IP not valid");
                        break;
                    }
                case 'f':
                    {   //Get Filename from "optarg"
                        fileName = parseFileName(optarg);
                        if (fileName == NULL) error("File not vaild");
                        break;
                    }
                case 'p':
                    {   // Get port number from "optarg"
                        port = parsePort(optarg);
                        if (port == -1) error("Port not valid");
                        break;
                    }
                case 'h':
                    {
                        usage();
                        return 0;
                    }
                case '?':
                    {
                        if ((optopt == 'p') || (optopt == 'f') || (optopt == 'a'))
                            {
                                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                            }
                        else if (isprint (optopt))
                            {
                                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                            }
                        else
                            {
                                fprintf (stderr,
                                         "Unknown option character `\\x%x'.\n",
                                         optopt);
                            }
                        error("Invalid input");
                        break;
                    }

                default:
                    {
                        usage();
                        error("Unrecognized input");
                        break;
                    }
                }
        }

    // Do we have all input?
    if (port == -1 || serverIP == NULL)
        {
            error("Missing vital parameter");
            return 1; // Hint to Lint, error exits.
        }

    if (fileName != NULL)
        {
            fp = fopen(fileName, "w+");
        }
    else
        {
            fp = stdout;
        }
    if (fp == 0) error("fopen() Failed");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    //printf("IP = %s\n", serverIP);
    //printf("port = %d\n", port);
    //printf("file = %s\n", fileName);


    if(inet_pton(AF_INET, serverIP, &serv_addr.sin_addr)<=0) error("inet_pton error occured");
    retval = connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (retval < 0)
        {
            int err = errno;
            printf("connect() failed with errno %d", err);
            error("connect() failed.");
       }

    retval = readXBytes(sockfd, (uint8_t *)&length, sizeof(length));

    if (retval != sizeof(length)) error("Failed FileSize");

    length = ntohl(length);
    //printf("Expected fileSize = %d\n", length);

    numRead = 0;
    if (fp != 0)
        {
            while (numRead < length)
                {
                    numToRead = (CHUNK_SIZE < (length - numRead) ? CHUNK_SIZE : (length - numRead));
                    retval = readXBytes(sockfd, buffer, numToRead);
                    if (retval != numToRead) error("Failed read");
                    fwrite(buffer, 1, numToRead, fp);
                    numRead += numToRead;
                    //printf("Tot read = %d\n", numRead);
                }
            fclose(fp);
        }
    close(sockfd);
    return 0;
}
