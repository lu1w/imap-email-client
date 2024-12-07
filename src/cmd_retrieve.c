#include <stdio.h>
#include <stdlib.h> 
#include "util.c"

#define RETRIEVE_TAG_SP "A00retrieve "
#define PEEK "BODY.PEEK[]"

void writeRetrieveCmd(int sockfd, char *messageNum);
// int readAndStoreBodyUntilTag(int sockfd, char **content, int *capacity); 
void printEmail(char *content, int contentLen);


/* Read response from the server.
    if read "RETRIEVE_TAG_SP + SP + OK" ==> success, exit with 0 
        read "* {n} FETCH (BODY[] {literal} ...)"
    else if read RETRIEVE_TAG_SP + SP + NO/BAD ==> failure, exit with 3
*/   
void retrieve(int sockfd, char *messageNum) {
    writeRetrieveCmd(sockfd, messageNum); 

    /* Read and store email content */
    int capacity = DEFAULT_BUFFER_SIZE; 
    char *content = malloc(capacity); 
    int contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, RETRIEVE_TAG_SP); 
    // for (int t=0; t<contentLen; t++) {
    //     fprintf(stdout, "%c", content[t]); 
    // }
    // fprintf(stderr, "- contentLen = %d\n", contentLen); 

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
    fprintf(stderr, "- retrieveMsg = %s\n", retrieveMsg); 

    /* Send retrieve message to the server */
    validatedWrite(sockfd, retrieveMsg, retrieveMsgLen, "Failure in writing RETRIEVE command to the server\n");
    free(retrieveMsg); 
}


/* Keep reading until reaching the tag; store all the content read into `content` */
// int readAndStoreBodyUntilTag(int sockfd, char **content, int *capacity) {
//     char response[DEFAULT_BUFFER_SIZE];
//     int tagged = 0;     // a flag either 0 or 1 to see if we successfully read the tag of the message
//     int success = 0;    // a flag either 0 or 1 to see if we successfully read the server response
//     int bracket = 0;    // count to see if all brackets are enclosed 
//     int isLineFinished = LINE_FINISHED; 

//     char *target = RETRIEVE_TAG_SP; 
//     int targetLen = strlen(RETRIEVE_TAG_SP); 
//     int cmp = 0;        // the index of the target message that we need to compare 
//     int i = 0;          // the index of the response from the server  
//     int n;              // the length of the response from the server 
//     int contentLen = 0;     // the number of character read into `content`

//     /* Keep reading until the tag is read */
//     while (!success) {
//         n = validatedRead(sockfd, response, DEFAULT_BUFFER_SIZE, "Failure in reading RETRIEVE responses from the server\n"); 
//         fprintf(stderr, "- response(%d) = %s\n", n, response);
//         // printf("%s", response);

//         if (!tagged) {
//             for (i=0; i<n; i++) {
//                 // fprintf(stderr, "checking %d -> %c\n", i, response[i]); 

//                 /* If read the tag, update flag and break */
//                 if (cmp >= targetLen) {
//                     tagged = 1; 
//                     targetLen = strlen(OK_MSG_SP); 
//                     fprintf(stderr, "-tagged-\n"); 
//                     break; 
//                 }

//                 /* Store the content of the email */
//                 if (contentLen >= *capacity) {
//                     // fprintf(stderr, "-realloc content-\n"); 
//                     *capacity *= 2; 
//                     *content = realloc(*content, *capacity); 
//                 }
//                 (*content)[contentLen] = response[i];
//                 // fprintf(stderr, "%c", content[contentLen]); 
//                 // fprintf(stdout, "%c", content[contentLen]); 
//                 contentLen++; 

//                 /* Check if all brackets are enclosed */
//                 if (bracket == 0) {
//                     // fprintf(stderr, "- bracket is 0, check = %c\n", response[i]); 
//                     /* Check if we are at the end of the line, so we know when to check for the tag */
//                     if (isLineFinished == LINE_NOT_FINISHED) {
//                         if (response[i] == '\r') { isLineFinished = LINE_ABOUT_TO_FINISH; } 
//                     } else if (isLineFinished == LINE_ABOUT_TO_FINISH) {
//                         if (response[i] == '\n') { 
//                             isLineFinished = LINE_FINISHED; 
//                             // fprintf(stderr, "-CRLF detected-\n"); 
//                         } 
//                         else { isLineFinished = LINE_NOT_FINISHED; }
//                     } else if (isLineFinished == LINE_FINISHED) {
//                         /* Check for finished tag */
//                         if (response[i] == target[cmp]) {
//                             cmp++; 
//                         } else {
//                             cmp = 0; 
//                             isLineFinished = LINE_NOT_FINISHED; 
//                         }
//                     }
//                 }

//                 /* Update bracket counts, so we know when to check for the tag */
//                 if (response[i] == '(') {
//                     bracket += 1; 
//                 } else if (response[i] == ')') {
//                     bracket -= 1; 
//                     assert(bracket >= 0); // should not have negative value for `bracket`
//                 } 
//             }
//         } 

//         /* Found the response tag, compare the response status after the tag */
//         if (tagged) {
//             fprintf(stderr, "-tagged: checking for ok-\n"); 
//             for (cmp=0; i+cmp<n; cmp++) {
//                 fprintf(stderr, "comparing response=%c and okmsg=%c\n", response[i+cmp], OK_MSG_SP[cmp]); 
//                 if (cmp >= targetLen) {
//                     /* OK - Successfully fetched */
//                     success = 1; 
//                     fprintf(stderr, "-ok!-\n"); 
//                     break; 
//                 }
//                 if (response[i+cmp] != OK_MSG_SP[cmp]) {
//                     /* Failure to perform the command, exit */
//                     fprintf(stderr, "-not ok!-\n"); 
//                     printf("%s", MESSAGE_NOT_FOUND); 
//                     exit(SERVER_ERROR); 
//                 }
//             }
//         }
//     }
//     assert(bracket==0);     // sanity check 

//     // for (int t=0; t<contentLen; t++) {
//     //     fprintf(stdout, "%c", (*content)[t]); 
//     // }

//     return contentLen; 
// }


/* Print the email content, start at the first '(' and end when this bracket is closed */
void printEmail(char *content, int contentLen) {
    // fprintf(stderr, "-start printing email-\n"); 
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
                fprintf(stderr, "-start the bracket at ch=%d-\n", ch); 
                /* Start of the message, read the number of messages */
                while (content[ch] != '\n') {
                    ch++;
                }
            }
        } 
    }
}

