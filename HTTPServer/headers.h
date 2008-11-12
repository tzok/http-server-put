/*
 * headers.h
 *
 *  Created on: 2008-10-24
 *      Author: chriss, tzok
 */

#ifndef headers_h
#define headers_h

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>//for getting file modification date
#include "bstring/bstrlib.h"

void strptime(const char *, const char *, struct tm *);//warning prevention

#endif /* headers_h */
