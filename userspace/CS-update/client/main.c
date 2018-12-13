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
#include "CS-defs.h"


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
  printf("Fetches watt and kwh from server");
  printf("\n");
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
 * A TCP socket is created and a connection is made to the specified port
 * and address.
 * Upon conection the following happens.
 * 1. A PowerReportStruct is read from the socket.
 * 2. Power and Consumption is written to files
 * 3. File and socket is closed
 *
 *
 * \param -h print help
 * \return 0 if successful, otherwise 1
 *
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int32_t opt, retval, numRead, numToRead, length;
    uint8_t buffer[CHUNK_SIZE];
    int sockfd;
    FILE *fd1;
    FILE *fd2;
    struct sockaddr_in serv_addr;
    char *serverIP = NULL;
    char *fileName = NULL;
    int16_t port = -1;
    PowerReportStruct report;

    char *powerFile="/tmp/power";
    char *consumptionFile="/tmp/consumption"

    /* Parse command line for required information */
    while ((opt = getopt(argc, argv, "h")) != -1)
      {
	switch (opt)
	  {
	  case 'h':
	    {
	      usage();
	      return 0;
	    }
	  default:
	    {
	      usage();
	      error("Unrecognized input");
	      break;
	    }
	  }
      }


    fd1 = fopen(powerFile, "w+");
    if (fd1 == 0) error("fopen() Failed");

    fd2 = fopen(consumptionFile, "w+");
    if (fd2 == 0) error("fopen() Failed");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, SRV_ADDRESS, &serv_addr.sin_addr)<=0) error("inet_pton error occured");
    retval = connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (retval < 0)
        {
            int err = errno;
            printf("connect() failed with errno %d", err);
            error("connect() failed.");
       }

    retval = readXBytes(sockfd, (uint8_t *)&report, sizeof(PowerReportStruct));

    if (retval != sizeof(PowerReportStruct)) error("Failed FileSize");

    printf("Power:       %d W", report.W);
    printf("Consumption: %d kWh", report.kWh);

    fprintf(fd1, "%d", report.W);
    fprintf(fd2, "%d", report.kWh);
    fclose(fd1);
    fclose(fd2);

    close(sockfd);
    return 0;
}
