#ifndef API_H
#define API_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 4322
#define BUFFER_SIZE 1024

int createServer();

#endif // API_H