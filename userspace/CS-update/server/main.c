/******************************************************************************/
/**
 * \file main.cpp
 *
 * \brief This is a TCP server that will provide the contents of a file
 *        when a connection is made on a specific port, both provided
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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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
    printf("file-trx-server is a program serve up files over a network\n");
    printf("p - port to listen on\n");
    printf("b - Estimate file size by counting bytes\n");
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

    file = fopen(providedName, "r");
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
 * Entrypoint and main loop for file-trx-server
 *
 * A TCP socket is opened and we start listening for incoming connections.
 * Upon conection the following happens.
 * 1. The file is opened and the size is determined
 * 2. The size is transmitted as a 32 bit number over the socket
 * 3. size number of bytes is read from the file and transmitted on the socket
 * 4. File and socket is closed
 *
 * Up to 5 clients can be queued up simultaneously
 *
 * \param -f file to dump data from
 * \param -p port to accept connections on
 * \param -h print help
 * \return Daemonizes
 *
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int sockfd, newsockfd;
    int32_t fileSize, txFileSize, numWritten, numToWrite, opt;
    FILE *fp;
    socklen_t clilen;
    uint8_t buffer[CHUNK_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    char *fileName = NULL;
    int16_t port = -1;
    int countBytes = 0;

    /* Parse command line for required information */
    while ((opt = getopt(argc, argv, "p:f:hb")) != -1)
        {
            switch (opt)
                {
                case 'f':
                    {   //Get Filename from "optarg"
                        fileName = parseFileName(optarg);
                        if (fileName == NULL)
                            {
                                error("File not vaild");
                            }

                        break;
                    }
                case 'p':
                    {   // Get port number from "optarg"
                        port = parsePort(optarg);
                        if (port == -1)
                            {
                                error("Port not valid");
                            }
                        break;
                    }
                case 'b':
                    {
		      countBytes = 1;
                        break;
                    }
                case 'h':
                    {
                        usage();
                        return 0;
                    }
                case '?':
                    {
                        if ((optopt == 'p') || (optopt == 'f'))
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
    if (port == -1 || fileName == NULL)
        {
            error("Missing vital parameter");
            return 1; // To tell Lint we are exiting, happens in error()
        }

    /* Daemonize */
    if (daemon(1,1) != 0) error("Failed to daemonize");

    /* Setup sockets to accept connections */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");

    /* Tell OS we are interested, and tell it to keep a queue of 5 clients */
    if (listen(sockfd,5) != 0) error("Failed to listen");

    /* Enter forever loop waiting to serve client requests */
    for (;;)
        {
            clilen = sizeof(cli_addr);
            newsockfd = accept(sockfd,
                               (struct sockaddr *) &cli_addr,
                               &clilen);
            if (newsockfd < 0) error("ERROR on accept");

            fp = fopen(fileName, "r");
            if (fp != NULL)
                {
		  if (countBytes)
		    {
		      fileSize = 0;
		      while( fgetc(fp) != EOF )
			fileSize += 1;
		      fseek(fp, 0, SEEK_SET);
		    }
		  else
		    {
		      fseek(fp, 0, SEEK_END); // seek to end of file
		      fileSize = ftell(fp); // get current file pointer
		      fseek(fp, 0, SEEK_SET); // seek back to beginning of file
		    }
                }
            else
                {
                    fileSize = 0;
                }

            txFileSize = htonl(fileSize);
            /* Transmit first block of protocol, the number of bytes to expect */
            n = write(newsockfd, &txFileSize ,sizeof(fileSize));

            if (n != sizeof(fileSize)) error("Error writing to socket");

            numWritten = 0;
            if (fp != NULL)
                {
                    while (numWritten < fileSize)
                        {
                            numToWrite = (fileSize - numWritten) < CHUNK_SIZE ? (fileSize - numWritten) : CHUNK_SIZE;
                            n = fread(buffer, 1, numToWrite, fp);
                            if (n != numToWrite)
                                {
                                    printf("ERROR reading file\n");
                                    break;
                                }
                            n = write(newsockfd, buffer ,numToWrite);
                            if (n != numToWrite)
                                {
                                    printf("ERROR writing to socket\n");
                                    break;
                                }
                            numWritten += numToWrite;
                        }
                }
            close(newsockfd);
            if (fp != NULL) fclose(fp);
        }
    close(sockfd);
    return 0;
}
