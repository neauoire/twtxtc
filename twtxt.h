/*
 * twtxtc: A twtxt client in C.
 * http://hub.darcs.net/dertuxmalwieder/twtxtc
 *
 * Licensed under the terms of the WTFPL.
 * You just DO WHAT THE FUCK YOU WANT TO.
 *
 * Uses the cJSON parser, licensed under the
 * terms of the MIT License.
 */

#include <time.h>
#include "cJSON/cJSON.h"

#ifdef _WIN32
/* We need the path length first. */
#include <stdlib.h>
#define MAXPATHLEN _MAX_PATH
#endif

char* wrkbuf;

typedef struct _tweet_t {
    struct tm datetime;
    char username[50];
    char text[512];
} tweet_t;


/* PATHS */

char homedir[MAXPATHLEN];
char configfilespec[MAXPATHLEN];
char twtxtfilespec[MAXPATHLEN];
char curlfilespec[MAXPATHLEN];


/* FUNCTIONS */

void showUsage(char* argv[]);
cJSON* getConfigFile(void);
const char* getConfigValue(const char* key, const char* defaultvalue);

void listFollowing(const char* followingJSON);
void follow(const char* followingJSON, const char* username, const char* URL);
void unfollow(const char* followingJSON, const char* username);
void tweetsort(tweet_t* tweets, int size);
