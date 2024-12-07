#include <stdio.h>
#include <stdlib.h> 
#include "util.c"

#define LIST_TAG_SP "A00list "
#define LIST_CMD "FETCH 1:* BODY.PEEK[HEADER.FIELDS (SUBJECT)]"

void writeListCmd(int sockfd); 
int printSubjects(char *content, int contentLen); 


/* Entry point of LIST command */
void list(int sockfd) {
    /* Send list command to the server */
    writeListCmd(sockfd); 

    /* Read the response from the server */
    int capacity = DEFAULT_BUFFER_SIZE; 
    char *content = malloc(capacity); 
    int contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, LIST_TAG_SP); 

    /* Print the subjects of the emails in the folder */
    printSubjects(content, contentLen); 

    free(content); 
    return; 
}


/* Send the list command to the server */
void writeListCmd(int sockfd) {
    /* Construct retrieve message */
    int listMsgLen = strlen(LIST_TAG_SP) + strlen(LIST_CMD) + strlen(CRLF); 
    char *listMsg = malloc(listMsgLen + 1); assert(listMsg); 
    strcpy(listMsg, LIST_TAG_SP); 
    strcat(listMsg, LIST_CMD); 
    strcat(listMsg, CRLF); 
    fprintf(stderr, "- listMsg = %s\n", listMsg); 

    /* Send retrieve message to the server */
    validatedWrite(sockfd, listMsg, listMsgLen, "Failure in writing RETRIEVE command to the server\n");
    free(listMsg); 
}


/* Print the header content, start at the first '(' and end when this bracket is closed. 
    Return the number of characters printed. 
 */
int printSubjects(char *content, int contentLen) {
    // fprintf(stderr, "-start printing email-\n"); 
    int printed = 0; 
    int bracket = 0; 
    int body = 0; 
    for (int ch=0; ch<contentLen; ch++) {
        fprintf(stderr, "%c", content[ch]); 
        // fprintf(stderr, "(%c %d)", content[ch], content[ch]); 

        /* If we found the ending-bracket of the email, stop printing */
        if (content[ch] == ')') {
            bracket--; 
            if (bracket == 0) {
                /* Finished reading the message */
                if (printed == 0) {
                    /* No subject header */
                    printf("%s", NULL_SUBJECT); 
                }
                printf("\n"); 
                continue; 
            }
        }

        if (bracket == 0) {
            if (content[ch] == '*' && content[ch+1] == ' ') {
                ch += 2;
                /* Print the message number */
                while (content[ch] >= '0' && content[ch] <= '9') {
                    printf("%c", content[ch]); 
                    ch++; 
                }
                printf(":"); 
                body = 0; 
                printed = 0; 
            }
        }

        /* If we are in the middle of the bracket, print the content after the header */
        else if (bracket > 0) {
            if (content[ch] != CRLF[0] && content[ch] != CRLF[1]) {
                if (!body && content[ch] == ':') {
                    body = 1; 
                    continue; 
                } else if (body) {
                    printf("%c", content[ch]); 
                } 
                printed += 1; 
            } 
        } 

        /* If we are reach an opening-bracket, start printing the email content if it is the first opening-bracket */
        if (content[ch] == '(') {
            bracket++; 
            if (bracket == 1) {
                fprintf(stderr, "-start the bracket at ch=%d-\n", ch); 
                /* Start of the message, read the number of messages */
                while (content[ch] != '\n') {
                    fprintf(stderr, "not new line: %c\n", content[ch]); 
                    ch++;
                }
            }
        } 
    }
    // if (printed > 0) {
    //     printf("\n"); 
    // }

    return printed; 
}