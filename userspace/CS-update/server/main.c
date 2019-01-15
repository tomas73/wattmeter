/******************************************************************************/
/**
 * \file main.cpp
 *
 * \brief This is a TCP server that will provide current and accumulated 
 *        power consumption
 *
 *        | 32 bit Watt | 32 bit kWh |
 *
 *
 * \author Tomas Rosenkvist
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
#include "CS-defs.h"

const char *logFileName = "/tmp/power-update-server.log";
const char *powerFileName = "/sys/tomas/gpio60/diffTime";
const char *consumptionFileName = "/sys/tomas/gpio60/numWattHours";
static int numRequests = 0;
static int numFails = 0;

#define SCALE (3600)


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
  printf("Num Requests = %d", numRequests);
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
  printf("Listen on port 9123 for connections");
  printf("\n");
}

/******************************************************************************/
/**
 *
 * Entrypoint and main loop for file-trx-server
 *
 * A TCP socket is opened and we start listening for incoming connections.
 * Upon conection the following happens.
 * 1. watt and kWh are reported
 * 2. File and socket is closed
 *
 * Up to 5 clients can be queued up simultaneously
 *
 * \param -h print help
 * \return Daemonizes
 *
 ******************************************************************************/
int main(int argc, char *argv[])
{
  int32_t opt;
  int sockfd, newsockfd;
  PowerReportStruct report;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  int n;
  FILE *fd;
  FILE *logFd;
  float diffTime;
  uint32_t wh;

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
  
    /* Daemonize */
  if (daemon(1,1) != 0) error("Failed to daemonize");
  
  
  /* Setup sockets to accept connections */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");
  
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(PORT);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
  
  /* Tell OS we are interested, and tell it to keep a queue of 5 clients */
  if (listen(sockfd,5) != 0) error("Failed to listen");
  
  /* Enter forever loop waiting to serve client requests */
  numRequests = 0;
  printf("Start to wait for connections");
  logFd = fopen(logFileName, "a+");
  fprintf(logFd, "Server #1 started\n");
  fclose(logFd);
  for (;;)
    {
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd,
			 (struct sockaddr *) &cli_addr,
			 &clilen);
      if (newsockfd < 0) 
	{
	  logFd = fopen(logFileName, "a+");
	  fprintf(logFd, "Error on accept()\n");
	  fclose(logFd);
	  numFails++;
	  continue;
	}
      numRequests++;
      
      fd = fopen(powerFileName, "r");
      fscanf(fd, "%f", &diffTime);
      fclose(fd);
      
      fd = fopen(consumptionFileName, "r");
      fscanf(fd, "%d", &wh);
      fclose(fd);
      
      report.W = (SCALE/diffTime);
      report.Wh = wh;
      
      
      /* Transmit first block of protocol, the number of bytes to expect */
      n = write(newsockfd, &report ,sizeof(report));
      // close(newsockfd);
      if (n != sizeof(report))
	{
	  logFd = fopen(logFileName, "a+");
	  fprintf(logFd, "Error on write()\n");
	  fclose(logFd);
	  numFails++;
	  continue;
	}
      logFd = fopen(logFileName, "a+");
      fprintf(logFd, "Num Requests=%d, numFails=%d\n", numRequests, numFails);
      fclose(logFd);
    }
  close(sockfd);
  return 0;
}
