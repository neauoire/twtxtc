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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#ifndef _WIN32
# include <sys/param.h>   /* MAXPATHLEN */
#else
# ifdef _MSC_VER
#   include <errno.h>     /* errno */
# endif
# ifndef NO_COLORS
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>    /* colored output */
# endif
#endif

#include "cJSON/cJSON.h"
#include "cJSON/cJSON_Utils.h" /* for sorting */
#include "twtxt.h"


int main(int argc, char *argv[]) {
    /* Find the home directory: */
#ifdef _MSC_VER
    char* buf = NULL;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, "USERPROFILE") == 0 && buf != NULL) {
        strcpy_s(homedir, MAXPATHLEN, buf);
        free(buf);
    }
#elif defined(_WIN32)
    strncpy(homedir, getenv("USERPROFILE"), MAXPATHLEN);
#else
    strncpy(homedir, getenv("HOME"), MAXPATHLEN);
#endif

    /* Parse arguments: */
    if (argc < 2) {
        /* Not even a command was specified. Show usage and exit. */
        showUsage(argv);
        return 1;
    }

    /* Find the default config file. */
#ifdef _WIN32
    strcpy_s(configfilespec, MAXPATHLEN, homedir);
    strcat_s(configfilespec, MAXPATHLEN, "\\.twtxtconfig");
#else
    strncpy(configfilespec, homedir, MAXPATHLEN);
    strncat(configfilespec, "/.twtxtconfig", MAXPATHLEN);
#endif

    /* Find the default twtxt file. Requires the config file. */
#ifdef _WIN32
    strcpy_s(twtxtfilespec, MAXPATHLEN, getConfigValue("twtxtfile", "twtxt.txt"));
#else
    strncpy(twtxtfilespec, getConfigValue("twtxtfile", "twtxt.txt"), MAXPATHLEN);
#endif

    /* Find the default cURL executable file. */
#ifdef _WIN32
    /* We might or might not be on the PowerShell here. Define curl.exe as the
       curlfilespec because the built-in "curl" alias is too unpredictable.

       Note: Does _popen() actually respect shell aliases? I might or might not
       want to revisit this in the future.
     */
    strcpy_s(curlfilespec, MAXPATHLEN, "curl.exe");
#else
    /* Meh. */
    strncpy(curlfilespec, "curl", MAXPATHLEN);
#endif

    /* argv[1] should be the command now. */
    const char* command = argv[1];

    if (strcmp(command, "tweet") == 0) {
        if (argc != 3) {
            /* No valid tweet specified. Show usage and exit. */
            puts("Invalid number of parameters.");
            showUsage(argv);
            return 1;
        }

        /* Add <tweet> to the twtxt file: */
        FILE* twtxtfile;
        const char* text = argv[2];

#ifdef _MSC_VER
        errno_t err;
        if ((err = fopen_s(&twtxtfile, twtxtfilespec, "a")) != 0) {
#else
        twtxtfile = fopen(twtxtfilespec, "a");
        if (twtxtfile == NULL) {
#endif
            /* Whoops... */
            printf("Could not open '%s' for writing.\n", twtxtfilespec);
            puts("Please check the access rights to the specified directory and try again.");
            if (wrkbuf != NULL) {
                /* Clean up first. */
                free(wrkbuf);
            }
            return 1;
        }

        /* Get the current time string: */
        char timebuf[26];
        time_t now;
        now = time(NULL);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S%z", localtime(&now));

        /* Add a colon in the timezone to match the official twtxt client format.
           Currently the string has 24 of the 25 available characters, the : needs
           to be inserted at pos. 23 (= timebuf[22]).
        */
        timebuf[22] = ':';
        timebuf[24] = '0';
        timebuf[sizeof(timebuf)-1] = '\0'; /* Terminate! */

        /* Build the new line and write it into the file: */
        fprintf(twtxtfile, "%s\t%s\n", timebuf, text);

        /* Done. */
        fclose(twtxtfile);
    }
    else if (strcmp(command, "timeline") == 0) {
        /* Fetch and display the timeline. */
        FILE* curl_exec;

        /* Check for curl availability: */
        char sCurlTestCommand[256];
        sprintf(sCurlTestCommand, "%s 2>&1", curlfilespec);
#ifdef _WIN32
        if ((curl_exec = _popen(sCurlTestCommand, "rt")) == NULL) {
#else
        if ((curl_exec = popen(sCurlTestCommand, "r")) == NULL) {
#endif
            puts("You don't seem to have a cURL executable in your $PATH. Mind to fix that?");
            if (wrkbuf != NULL) {
                /* Clean up before returning. */
                free(wrkbuf);
            }
            return 1;
        }

        /* Fetch all timeline txt files: */
        cJSON* followingList = cJSON_Parse(getConfigValue("following", NULL));
        if (followingList != NULL && cJSON_GetArraySize(followingList) > 0) {
            tweet_t *ptr = malloc(sizeof(tweet_t));
            cJSON *followingIter = followingList->child;

            size_t iAllTweets = 0;
            size_t iLongestNickname = 0;

            while (followingIter) {
                /* followingIter->string = the user name
                   followingIter->valuestring = the URL

                   Try to retrieve all "timelines" and add them to
                   the tweet list:
                */

                char sCurlCommand[256];
                sprintf(sCurlCommand, "%s -s %s", curlfilespec, followingIter->valuestring);
#ifdef _WIN32
                curl_exec = _popen(sCurlCommand, "rt");
#else
                curl_exec = popen(sCurlCommand, "r");
#endif
                char output[512];
                while (!feof(curl_exec)) {
                    if (fgets(output, sizeof(output), curl_exec) != NULL) {
                        /* Analyze <output> which should contain a valid twtxt tweet now: */
                        int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0, tzh = 0, tzm = 0;
                        int dummy = 0;
                        char tweet[500]; /* or whatevs */

                        /* There are numerous ways for formatting timestamps in twtxt (because lol standards). */
                        char* style1 = "%4d-%2d-%2dT%2d:%2d:%2d+%2d:%2d\t%[^\n]"; /* yyyy-mm-ddThh:mm:ss+hh:mm\tTEXT */
                        char* style2 = "%4d-%2d-%2dT%2d:%2d:%2d.%6d+%2d:%2d\t%[^\n]"; /* yyyy-mm-ddThh:mm:ss.millisecs+hh:mm\tTEXT */
                        char* style3 = "%4d-%2d-%2dT%2d:%2d:%2dZ\t%[^\n]"; /* yyyy-mm-ddThh:mm:ssZ\tTEXT */
                        char* style4 = "%4d-%2d-%2dT%2d:%2d:%2d.%dZ\t%[^\n]"; /* yyyy-mm-ddThh:mm:ss.msZ\tTEXT */
                        char* style5 = "%4d-%2d-%2dT%2d:%2d:%2d.%6dZ\t%[^\n]"; /* yyyy-mm-ddThh:mm:ss.msZ\tTEXT */

                        /* Regex would probably be easier to handle here, but adds additional dependencies on some platforms. */
                        if (sscanf(output, style1, &year, &month, &day, &hour, &min, &sec, &tzh, &tzm, tweet) != 9) {
                            if (sscanf(output, style2, &year, &month, &day, &hour, &min, &sec, &dummy, &tzh, &tzm, tweet) != 10) {
                                if (sscanf(output, style3, &year, &month, &day, &hour, &min, &sec, tweet) != 7) {
                                    if (sscanf(output, style4, &year, &month, &day, &hour, &min, &sec, &dummy, tweet) != 8) {
                                        if (sscanf(output, style5, &year, &month, &day, &hour, &min, &sec, &dummy, tweet) != 8) {
                                            /* Not a valid twtxt tweet. */
                                            continue;
                                        }
                                    }
                                }
                            }
                        }

                        /* Filter out time trolls: */
                        time_t now = time(0);
                        struct tm * timeinfo = localtime(&now);
                        if (timeinfo->tm_year + 1902 < year) {
                            /* No chance. */
                            continue;
                        }

                        if (strlen(followingIter->string) > iLongestNickname) {
                            /* We should only check the length of users who actually tweeted recently.
                               The others are invisible to the user here.
                            */
                            iLongestNickname = strlen(followingIter->string);
                        }

                        /* Mentions to someone else than me are against the, uhm, law or so. */
                        char mention[100];
                        char url[256];
                        char mentiontext[500];
                        char initialtext[500];

                        if (sscanf(tweet, "@<%s %[^>]> %s", mention, url, mentiontext) == 3) {
                            if (strncmp(mention, getConfigValue("nickname", ""), strlen(mention)) != 0) {
                                /* Nope, not me. */
                                continue;
                            }
                            else {
                                /* Reformat. */
                                char newtweet[500];
                                sprintf(newtweet, "@%s %s", mention, mentiontext);
#ifdef _MSC_VER
                                strcpy_s(tweet, sizeof(tweet), newtweet);
#else
                                strncpy(tweet, newtweet, sizeof(tweet));
#endif
                            }
                        }

                        /* Reformat inline mentions as well: */
                        if (sscanf(tweet, "%[^@] @<%s %[^>]> %s", initialtext, mention, url, mentiontext) == 4) {
                            /* Reformat. */
                            char newtweet[500];
                            sprintf(newtweet, "%s @%s %s", initialtext, mention, mentiontext);
#ifdef _MSC_VER
                            strcpy_s(tweet, sizeof(tweet), newtweet);
#else
                            strncpy(tweet, newtweet, sizeof(tweet));
#endif
                        }

                        struct tm tweettime = {
                            .tm_year = year - 1900,
                            .tm_mon = month - 1,
                            .tm_mday = day,
                            .tm_hour = hour + tzh,
                            .tm_min = min,
                            .tm_sec = sec
                        };
                        ptr[iAllTweets].datetime = tweettime;

#ifdef _MSC_VER
                        strcpy_s(ptr[iAllTweets].username, 50, followingIter->string);
                        strcpy_s(ptr[iAllTweets].text, 512, tweet);
#else
                        strncpy(ptr[iAllTweets].username, followingIter->string, 50);
                        strncpy(ptr[iAllTweets].text, tweet, 512);
#endif
                       iAllTweets++;

                        /* Add space for more tweets: */
                        tweet_t *tmp_ptr;
                        tmp_ptr = realloc(ptr, (iAllTweets + 1) * sizeof(tweet_t));
                        if (tmp_ptr == NULL) {
                            /* :-( */
                            puts("realloc() failed. Panic!");
                            if (wrkbuf != NULL) {
                                /* Don't panic without a cleanup though. */
                                free(wrkbuf);
                            }
                            return 1;
                        } else {
                            // everything went ok, update the original pointer (temp_struct)
                            ptr = tmp_ptr;
                        }
                    }
                }

                followingIter = followingIter->next;
            }

            /* We need to rearrange the array by sorting by mktime(datetime),
               then print the top <maxVal> entries with their user name: */
            tweetsort(ptr, iAllTweets);

            /* We probably have a sorted ptr now. Get its bottom <maxlog>. */
            puts("");
            double maxVal = MIN(atof(getConfigValue("maxlog", "100")), iAllTweets);
            printf("These are your newest %d tweets:\n\n", (int) maxVal);
            const char* spacing = getConfigValue("spacing", "   ");

            int i = 0; /* loop variable */
            int j = 0; /* limiter to maxVal */
            for (i = iAllTweets; i >= 0, j <= maxVal; --i, ++j) {
                /* Print the newest tweets (user<spacing>tweet). */
                if (strlen(ptr[i].username) > iLongestNickname) {
                    /* Faulty line. */
                    continue;
                }

#ifdef NO_COLORS
                /* In no-color mode, the user name is prefixed with "@". */
                printf("@%*s%s%s\n", (int) iLongestNickname, ptr[i].username, spacing, ptr[i].text);
#else
# ifdef _WIN32
                /* The user prefers colorful text. Hooray. Let's fire up the Windows API! */
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
                WORD saved_attributes;

                /* Make the user name yellow and the text white: */
                GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
                saved_attributes = consoleInfo.wAttributes;

                SetConsoleTextAttribute(hConsole, 14); /* 14 = yellow because wtf */
                printf("@%*s", (int) iLongestNickname, ptr[i].username);

                /* Restore the original values. */
                SetConsoleTextAttribute(hConsole, saved_attributes);
                printf("%s", spacing);

                SetConsoleTextAttribute(hConsole, 15); /* 15 = white because srsly */
                printf("%s\n", ptr[i].text);

                /* Restore the original values again. */
                SetConsoleTextAttribute(hConsole, saved_attributes);
# else
                /* POSIX colors are not really more convenient. */
#define YELLOW "\x1B[33m" /* lol */
#define WHITE  "\x1B[37m" /* actually ... */
#define RESET  "\x1B[0m"  /* WHY?! */
                printf(YELLOW "@%*s" RESET "%s" WHITE "%s\n" RESET, (int) iLongestNickname, ptr[i].username, spacing, ptr[i].text);
# endif
#endif
            }

            free(ptr);
            cJSON_Delete(followingList);
        }

#ifdef _WIN32
        _pclose(curl_exec);
#else
        pclose(curl_exec);
#endif
    }
    else if (strcmp(command, "following") == 0) {
        /* Fetch and display the subscribed users. */
        listFollowing(getConfigValue("following", NULL));
        if (wrkbuf != NULL) {
            puts(wrkbuf);
        }
    }
    else if (strcmp(command, "follow") == 0) {
        if (argc != 4) {
            /* No valid user+URL specified. Show usage and exit. */
            puts("Invalid number of parameters.");
            showUsage(argv);
            return 1;
        }

        follow(getConfigValue("following", NULL), argv[2], argv[3]);
    }
    else if (strcmp(command, "unfollow") == 0) {
        if (argc != 3) {
            /* No valid user name specified. Show usage and exit. */
            puts("Invalid number of parameters.");
            showUsage(argv);
            return 1;
        }

        unfollow(getConfigValue("following", NULL), argv[2]);
    }
    else if (strcmp(command, "help") == 0) {
        /* Display the help and exit. */
        showUsage(argv);
        return 0;
    }
    else {
        /* Invalid command specified. Show usage and exit. */
        printf("Invalid command: %s\n", command);
        showUsage(argv);
        return 1;
    }

    if (wrkbuf != NULL) {
        free(wrkbuf);
    }
    return 0;
}


void showUsage(char* argv[]) {
    /* Displays the available command-line flags. */
    puts("");
    puts("Usage:");
    printf("\t%s [COMMAND]\n", argv[0]);
    puts("");
    puts("Commands:");
    puts("\ttweet <text>\t\tAdds <text> to your twtxt timeline.");
    puts("\ttimeline\t\tDisplays your twtxt timeline.");
    puts("\tfollowing\t\tGives you a list of all people you follow.");
    puts("\tfollow <user> <URL>\tAdds the twtxt file from <URL> to your timeline.");
    puts("\t\t\t\t<user> defines the user name to display.");
    puts("\tunfollow <user>\t\tRemoves the user with the display name <user> from your timeline.");
    puts("\thelp\t\t\tDisplays this help screen.\n");
    puts("Configuration:");
    puts("\ttwtxtc uses the following configuration file:");
#ifdef _WIN32
    printf("\t%s\\.twtxtconfig", homedir);
#else
    printf("\t%s/.twtxtconfig", homedir);
#endif
    puts("\n");
}


void tweetsort(tweet_t* tweets, int size) {
    /* Quicksort over <tweets> according to mktime(datetime). */
    if (size < 2) { return; }
 
    int pivot = (int) mktime(&tweets[size / 2].datetime);
 
    int i, j;
    for (i = 0, j = size - 1; ; i++, j--) {
        while (mktime(&tweets[i].datetime) < pivot) { i++; }
        while (mktime(&tweets[j].datetime) > pivot) { j--; }
 
        if (i >= j) { break; }
 
        tweet_t temp = tweets[i];
        tweets[i] = tweets[j];
        tweets[j] = temp;
    }
 
    tweetsort(tweets, i);
    tweetsort(tweets + i, size - i);
}


cJSON* getConfigFile(void) {
    /* Returns the config. file as JSON object or NULL on errors. */
    FILE *configfile;
    char* configstring;
    long lSize;

    /* Read the config file: */
    configfile = fopen(configfilespec, "ab+");
    if (!configfile) {
        /* Failed to read the file, suddenly...? */
        printf("Could not open '%s'.\n", configfilespec);
        return NULL;
    }

    fseek(configfile, 0L, SEEK_END);
    lSize = ftell(configfile);
    rewind(configfile);

    configstring = calloc(1, lSize + 1);
    if (!configstring) {
        /* Allocation failed. */
        puts("Memory allocation failed.");
        fclose(configfile);
        return NULL;
    }

    if (lSize > 0 && fread(configstring, lSize, 1, configfile) != 1) {
        /* File reading failed. */
        fclose(configfile);
        free(configstring);
        puts("Could not read from the configuration file. You should check that.");
        return NULL;
    }

    fclose(configfile);

    /* Now parse the JSON. */
    cJSON* ret = cJSON_Parse(lSize > 0 ? configstring : "{}");
    free(configstring);

    return ret;
}


const char* getConfigValue(const char* key, const char* defaultvalue) {
    /* Reads the configuration value from <key>.
     * Returns <defaultvalue> if the configuration file does not
     * exist or the key could not be found. */
    cJSON* root = getConfigFile();

    if (root == NULL) {
        /* Invalid configuration file, probably. */
        puts("Please validate your .twtxtconfig format - we could not parse it. :-(");
        return defaultvalue;
    }

    if (strcmp(key, "following") == 0) {
        /* The list of users you follow is an object. Return it as such. */
        return cJSON_Print(cJSON_GetObjectItem(root, "following"));
    }

    /* Parse the configuration file. */
    char strvalue[512];

    cJSON *jsonValue = cJSON_GetObjectItem(root, key);
    if (jsonValue == NULL) {
        /* We don't have this value. Return what was said to be normal. */
        return defaultvalue;
    }

    /* Allocate some space: */
    wrkbuf = (char *)malloc(sizeof(strvalue));

#ifdef _MSC_VER
    if (cJSON_IsNumber(jsonValue)) {
        sprintf_s(strvalue, sizeof(strvalue), "%f", jsonValue->valuedouble);
    }
    else {
        sprintf_s(strvalue, sizeof(strvalue), "%s", jsonValue->valuestring);
    }
    strcpy_s(wrkbuf, sizeof(strvalue), strvalue);
#else
    if (cJSON_IsNumber(jsonValue)) {
        sprintf(strvalue, "%f", jsonValue->valuedouble);
    }
    else {
        sprintf(strvalue, "%s", jsonValue->valuestring);
    }
    strncpy(wrkbuf, strvalue, sizeof(strvalue));
#endif

    /* Cleanup */
    cJSON_Delete(root);

    return wrkbuf == NULL ? defaultvalue : wrkbuf;
}


void listFollowing(const char* followingJSON) {
    /* Puts the list of users you follow into <wrkbuf>. */
    cJSON *following = cJSON_Parse(followingJSON);

    if (following == NULL || cJSON_GetArraySize(following) == 0) {
        puts("You are not following any users yet.\n");
        return;
    }

    /* There actually are people you follow. */
    wrkbuf = (char *)malloc(4096); /* 4 KiB should be enough for now */
#ifdef _MSC_VER
    strcpy_s(wrkbuf, 4096, "");
#else
    strncpy(wrkbuf, "", 4096);
#endif

    puts("");
    puts("You are following these users:");

    /* Sort the list first: */
    cJSONUtils_SortObject(following);

    cJSON *followinglist = following->child;
    while (followinglist) {
        /* Traverse through the list. */
        char strvalue[400];

#ifdef _MSC_VER
        sprintf_s(strvalue, sizeof(strvalue), "  @%-20s [ %s ]\n", followinglist->string, followinglist->valuestring);
        strcat_s(wrkbuf, sizeof(strvalue), strvalue);
#else
        sprintf(strvalue, "  @%-20s [ %s ]", followinglist->string, followinglist->valuestring);
        strncat(wrkbuf, strvalue, sizeof(wrkbuf));
#endif
        followinglist = followinglist->next;
    }

    cJSON_Delete(followinglist);
    cJSON_Delete(following);
}


void follow(const char* followingJSON, const char* username, const char* URL) {
    /* Tries to follow <username>. */
    cJSON *following = cJSON_Parse(followingJSON);

    int hasFollowingList = 1;
    if (following == NULL) {
        /* This would be the first user to follow ... */
        following = cJSON_CreateObject();
        hasFollowingList = 0;
    }

    /* Avoid duplicates. */
    cJSON *followinglist = following->child;

    while (followinglist) {
        /* followinglist->string = the user name */
        if (strncmp(followinglist->string, username, strlen(followinglist->string)) == 0) {
            /* This is the user. */
            puts("You already follow this user.");

            /* Clean up. */
            cJSON_Delete(following);
            return;
        }

        followinglist = followinglist->next;
    }

    /* Add the data. */
    cJSON_AddStringToObject(following, username, URL);

    /* Write the list back. */
    cJSON* cjconfig = getConfigFile();
    if (cjconfig != NULL) {
        if (hasFollowingList == 0) {
            /* The user had no following list up to this point. Add one. */
            cJSON_AddItemToObject(cjconfig, "following", following);
        }
        else {
            cJSON_ReplaceItemInObject(cjconfig, "following", following);
        }
        FILE* configfile;
#ifdef _MSC_VER
        errno_t err;
        if ((err = fopen_s(&configfile, configfilespec, "w")) != 0) {
#else
        configfile = fopen(configfilespec, "w");
        if (configfile == NULL) {
#endif
            /* Whoops... */
            printf("Could not open '%s' for writing.\n", configfilespec);
            puts("Please check the access rights and try again.");
        }
        else {
            fprintf(configfile, "%s", cJSON_Print(cjconfig));
            fclose(configfile);

            printf("You follow @%s now.\n", username);
        }

        free(cjconfig);
    }

    cJSON_Delete(following);
}

void unfollow(const char* followingJSON, const char* username) {
    /* Tries to unfollow <username>. */
    cJSON *following = cJSON_Parse(followingJSON);

    if (following == NULL) {
        puts("You are not following any users yet.");
        return;
    }

    cJSON *followinglist = following->child;
    cJSON *newList = cJSON_CreateObject();
    int foundUser = 0;

    while (followinglist) {
        /* followinglist->string = the user name */
        if (strncmp(followinglist->string, username, strlen(followinglist->string)) == 0) {
            /* This is the user. Skip it. */
            foundUser = 1;
            followinglist = followinglist->next;
            continue;
        }

        /* Build a new list by copying the old one minus the user to unfollow. */
        cJSON_AddStringToObject(newList, followinglist->string, followinglist->valuestring);
        followinglist = followinglist->next;
    }

    if (!foundUser) {
        /* We can't unfollow a user we don't follow. */
        puts("You do not follow this user.");
    }
    else {
        /* Write newList back into the configuration file. */
        cJSON* cjconfig = getConfigFile();
        if (cjconfig != NULL) {
            cJSON_ReplaceItemInObject(cjconfig, "following", newList);
            FILE* configfile;
#ifdef _MSC_VER
            errno_t err;
            if ((err = fopen_s(&configfile, configfilespec, "w")) != 0) {
#else
            configfile = fopen(configfilespec, "w");
            if (configfile == NULL) {
#endif
                /* Whoops... */
                printf("Could not open '%s' for writing.\n", configfilespec);
                puts("Please check the access rights and try again.");
            }
            else {
                fprintf(configfile, "%s", cJSON_Print(cjconfig));
                fclose(configfile);
            }
        }

        free(cjconfig);
    }
    cJSON_Delete(following);
    cJSON_Delete(newList);
}
