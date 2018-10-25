#include <stdio.h>

#define SCALE (3600)

const char *fileName = "/sys/tomas/gpio60/diffTime";

int main(void)
{
  FILE *fd;
  float diffTime;
  int power;

  fd = fopen(fileName, "r");
  fscanf(fd, "%f", &diffTime);

  power = (SCALE/diffTime);
  printf("%d\n", power);
  fclose(fd);
}
