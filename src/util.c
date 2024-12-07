#include <stdlib.h>
#include <stdio.h> 
#include <unistd.h>

#define DEFAULT_BUFFER_SIZE 1024

#define SP " "
#define CRLF "\r\n"
#define OK_MSG_SP "OK "

#define FETCH_MSG_SP "FETCH "
#define MESSAGE_NOT_FOUND "Message not found\n"
#define NULL_SUBJECT " <No subject>"

#define CMD_ARG_ERROR 1 
#define CONNECTION_ERROR 2
#define SERVER_ERROR 3
#define RESPONSE_ERROR 4
#define GENERAL_ERROR 5 

#define LINE_NOT_FINISHED -2
#define LINE_ABOUT_TO_FINISH -1
#define LINE_FINISHED 0 

#ifndef UTIL_H
#define UTIL_H

int validatedRead(int sockfd, char *response, int size, char *error) {
    fprintf(stderr, ":Start reading from the server\n"); 
    int n = read(sockfd, response, size); 
    fprintf(stderr, ":Finished reading from the server\n"); 
    if (n < 0) {
        //char error[strlen("")]
        printf("%s", error);
        exit(SERVER_ERROR);
    }
    return n;
}

int validatedWrite(int sockfd, char *msg, int size, char *error) {
    int n = write(sockfd, msg, size);
	if (n < 0) {
		printf("%s", error);
        exit(SERVER_ERROR);
	}
    return n;
}

void assertMalloc(void *memory) {
    if (!memory) {
        printf("Failure in Memory Allocation\n"); 
        exit(GENERAL_ERROR); 
    }
}

/* Keep reading until reaching the tag; store all the content read into `content`.
    If the server response status is not "OK", exit. 
 */
int readAndStoreUntilTag(int sockfd, char **content, int *capacity, char *tag) {
    char response[DEFAULT_BUFFER_SIZE];
    int tagged = 0;     // a flag either 0 or 1 to see if we successfully read the tag of the message
    int success = 0;    // a flag either 0 or 1 to see if we successfully read the server response
    int bracket = 0;    // count to see if all brackets are enclosed 
    int isLineFinished = LINE_FINISHED; 

    int tagLen = strlen(tag); 
    int cmp = 0;        // the index of the target message that we need to compare 
    int i = 0;          // the index of the response from the server  
    int n;              // the length of the response from the server 
    int contentLen = 0;     // the number of character read into `content`

    fprintf(stderr, "- tag=%s\n", tag); 

    /* Keep reading until the tag is read */
    while (!success) {
        fprintf(stderr, "not suceess, keep reading next slot\n");
        n = validatedRead(sockfd, response, DEFAULT_BUFFER_SIZE, "Invalid read from the server\n"); 
        fprintf(stderr, "\n----------------:\n- response(%d) = %s\n----------------\n", n, response);
        // printf("%s", response);

        if (!tagged) {
            for (i=0; i<n; i++) {
                //fprintf(stderr, "checking %d -> %c\n", i, response[i]); 

                /* If read the tag, update flag and break */
                if (cmp >= tagLen) {
                    tagged = 1; 
                    tagLen = strlen(OK_MSG_SP); 
                    fprintf(stderr, "-tagged-\n"); 
                    break; 
                }

                /* Store the content of the email */
                if (contentLen >= *capacity) {
                    // fprintf(stderr, "-realloc content-\n"); 
                    *capacity *= 2; 
                    *content = realloc(*content, *capacity); 
                }
                (*content)[contentLen] = response[i];
                // fprintf(stderr, "%c", content[contentLen]); 
                // fprintf(stdout, "%c", content[contentLen]); 
                contentLen++; 

                /* Check if all brackets are enclosed */
                if (bracket == 0) {
                    // fprintf(stderr, "- bracket is 0, check = %c\n", response[i]); 

                    /* Check if we are at the end of the line, so we know when to check for the tag */
                    if (isLineFinished == LINE_NOT_FINISHED) {
                        if (response[i] == '\r') { isLineFinished = LINE_ABOUT_TO_FINISH; } 
                    } else if (isLineFinished == LINE_ABOUT_TO_FINISH) {
                        if (response[i] == '\n') { 
                            isLineFinished = LINE_FINISHED; 
                            // fprintf(stderr, "-CRLF detected-\n"); 
                        } 
                        else { isLineFinished = LINE_NOT_FINISHED; }
                    } else if (isLineFinished == LINE_FINISHED) {
                        /* Check for finished tag */
                        if (response[i] == tag[cmp]) {
                            cmp++; 
                        } else {
                            cmp = 0; 
                            isLineFinished = LINE_NOT_FINISHED; 
                        }
                    }
                }

                /* Update bracket counts, so we know when to check for the tag */
                if (response[i] == '(') {
                    bracket += 1; 
                } else if (response[i] == ')') {
                    bracket -= 1; 
                    assert(bracket >= 0); // should not have negative value for `bracket`
                } 
            }
        } 

        /* Found the response tag, compare the response status after the tag */
        fprintf(stderr, "-checking for ok, tagged=%d, success=%d-\n", tagged, success); 
        if (tagged) {
            fprintf(stderr, "-tagged: checking for ok-\n"); 
            for (cmp=0; i+cmp<n; cmp++) {
                fprintf(stderr, "comparing response=%c and okmsg=%c\n", response[i+cmp], OK_MSG_SP[cmp]); 
                if (cmp >= tagLen) {
                    /* OK - Successfully fetched */
                    success = 1; 
                    fprintf(stderr, "-ok!-\n"); 
                    break; 
                }
                if (response[i+cmp] != OK_MSG_SP[cmp]) {
                    /* Failure to perform the command, exit */
                    fprintf(stderr, "-not ok!-\n"); 
                    printf("%s", MESSAGE_NOT_FOUND); 
                    exit(SERVER_ERROR); 
                }
            }
        }
    }
    assert(bracket==0);     // sanity check 

    // for (int t=0; t<contentLen; t++) {
    //     fprintf(stdout, "%c", (*content)[t]); 
    // }

    return contentLen; 
}

int max(int x, int y) {
    if (x > y) {
        return x;
    }
    return y;
}

#endif