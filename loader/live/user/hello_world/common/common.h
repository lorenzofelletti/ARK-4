#ifndef COMMON_H
#define COMMON_H

#define true  1
#define false 0
#define bool  unsigned char
#define NULL  0

#define INDEX_BLACK      10
#define INDEX_WHITE      1
#define INDEX_PINK       2
#define INDEX_RED        3
#define INDEX_BLUE       4
#define INDEX_GREEN      5
#define INDEX_YELLOW     6
#define INDEX_CYAN       7
#define INDEX_GRAY       9
#define INDEX_DARKGREEN  0
#define RANDOM           rand()%9


void memcpy(char *dest, char *src, int size);

int strlen(char * xistr);

void memdump();

int Sleep(unsigned int xicount);


#endif
