#include <stdio.h>
#include <stdlib.h> 
#include "util.c"

#define RETRIEVE_TAG_SP "A00retrieve "
#define PEEK "BODY.PEEK[]"

void writeRetrieveCmd(int sockfd, char *messageNum);
void printEmail(char *content, int contentLen);


void retrieve(int sockfd, char *messageNum) {
    writeRetrieveCmd(sockfd, messageNum); 

    /* Read and store email content */
    int capacity = DEFAULT_BUFFER_SIZE; 
    char *content = malloc(capacity); 
    int contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, RETRIEVE_TAG_SP); 

    /* Print the email */
    printEmail(content, contentLen); 

    free(content); 
}


/* Send the retrieve command to the server */
void writeRetrieveCmd(int sockfd, char *messageNum) {
    /* Construct retrieve message */
    int retrieveMsgLen = strlen(RETRIEVE_TAG_SP) + strlen(FETCH_MSG_SP) + strlen(messageNum) + strlen(SP) + strlen(PEEK) + strlen(CRLF); 
    char *retrieveMsg = malloc(retrieveMsgLen + 1); assert(retrieveMsg); 
    strcpy(retrieveMsg, RETRIEVE_TAG_SP); 
    strcat(retrieveMsg, FETCH_MSG_SP); 
    strcat(retrieveMsg, messageNum); 
    strcat(retrieveMsg, SP); 
    strcat(retrieveMsg, PEEK); 
    strcat(retrieveMsg, CRLF); 

    /* Send retrieve message to the server */
    validatedWrite(sockfd, retrieveMsg, retrieveMsgLen, "Failure in writing RETRIEVE command to the server\n");
    free(retrieveMsg); 
}


/* Print the email content, start at the first '(' and end when this bracket is closed */
void printEmail(char *content, int contentLen) {
    int bracket = 0; 
    for (int ch=0; ch<contentLen; ch++) {
        // fprintf(stdout, "%c", content[ch]); 

        /* If we found the ending-bracket of the email, stop printing; 
            assuming there is ever going to be at most one message as discussed in ed#856 */
        if (content[ch] == ')') {
            bracket--; 
            if (bracket == 0) {
                /* Finished reading the message */
                break; 
            }
        }

        /* If we are in the middle of the bracket, print the content */
        if (bracket > 0) {
            printf("%c", content[ch]); 
        } 

        /* If we are reach an opening-bracket, start printing the email content if it is the first opening-bracket */
        if (content[ch] == '(') {
            bracket++; 
            if (bracket == 1) {
                /* Start of the message, read the number of messages */
                while (content[ch] != '\n') {
                    ch++;
                }
            }
        } 
    }
}

