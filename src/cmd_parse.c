#include <stdio.h>
#include <stdlib.h> 
#include "util.c"

#define PARSE_FROM_TAG_SP "A00parseFROM "
#define PARSE_TO_TAG_SP "A00parseTO "
#define PARSE_DATE_TAG_SP "A00parseDATE "
#define PARSE_SUBJECT_TAG_SP "A00parseSUBJECT "

#define PEEK_HEADER_FROM "BODY.PEEK[HEADER.FIELDS (FROM)]" 
#define PEEK_HEADER_TO "BODY.PEEK[HEADER.FIELDS (TO)]"
#define PEEK_HEADER_DATE "BODY.PEEK[HEADER.FIELDS (DATE)]"
#define PEEK_HEADER_SUBJECT "BODY.PEEK[HEADER.FIELDS (SUBJECT)]"

#define HEADER_FROM "From:"
#define HEADER_TO "To:"
#define HEADER_DATE "Date:"
#define HEADER_SUBJECT "Subject:"

/* 
Output: 
    From: mailbox-list
    To: address-list 
    Date: date-time 
    Subject: unstructured

IMAP commands such as BODY.PEEK[HEADER.FIELDS (FROM)] or ENVELOPE that explicitly fetch header fields
*/

void writeParseCmd(int sockfd, char *messageNum, char *tag, char *cmd); 
int readAndStoreUntilTag(int sockfd, char **content, int *capacity, char *target); 
int printHeaders(char *header, char *content, int contentLen); 

void parse(int sockfd, char *messageNum) {
    int capacity = DEFAULT_BUFFER_SIZE; 
    int contentLen; 
    char *content = malloc(capacity); 

    /* Read FROM */
    writeParseCmd(sockfd, messageNum, PARSE_FROM_TAG_SP, PEEK_HEADER_FROM); 
    contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, PARSE_FROM_TAG_SP); 
    printHeaders(HEADER_FROM, content, contentLen); 

    /* Read TO */
    writeParseCmd(sockfd, messageNum, PARSE_TO_TAG_SP, PEEK_HEADER_TO);
    contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, PARSE_TO_TAG_SP); 
    if (printHeaders(HEADER_TO, content, contentLen) == 0) {
        printf("\n"); 
    }
    
    /* Read DATE */
    writeParseCmd(sockfd, messageNum, PARSE_DATE_TAG_SP, PEEK_HEADER_DATE);
    contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, PARSE_DATE_TAG_SP); 
    printHeaders(HEADER_DATE, content, contentLen); 

    /* Read SUBJECT */
    writeParseCmd(sockfd, messageNum, PARSE_SUBJECT_TAG_SP, PEEK_HEADER_SUBJECT);
    contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, PARSE_SUBJECT_TAG_SP); 
    if (printHeaders(HEADER_SUBJECT, content, contentLen) == 0) {
        printf("%s\n", NULL_SUBJECT); 
    }

    free(content); 
}

// void parseTo(int sockfd, char *messageNum) {
//     int capacity = 1024; 
//     int contentLen; 
//     char *content = malloc(capacity); 

//     /* Read TO */
//     writeParseCmd(sockfd, messageNum, PARSE_TO_TAG_SP, PEEK_HEADER_TO);
//     contentLen = readAndStoreUntilTag(sockfd, &content, &capacity, PARSE_TO_TAG_SP); 
//     printHeaders(content, contentLen); 

//     free(content); 
// }

void writeParseCmd(int sockfd, char *messageNum, char *tagSP, char *cmd) {
    /* Construct parse message */
    int parseMsgLen = strlen(tagSP) + strlen(FETCH_MSG_SP) + strlen(messageNum) + strlen(SP) + strlen(cmd) + strlen(CRLF); 
    char *parseMsg = malloc(parseMsgLen + 1); assert(parseMsg); 
    strcpy(parseMsg, tagSP); 
    strcat(parseMsg, FETCH_MSG_SP); 
    strcat(parseMsg, messageNum); 
    strcat(parseMsg, SP); 
    strcat(parseMsg, cmd); 
    strcat(parseMsg, CRLF); 
    fprintf(stderr, "- parseMsg = %s\n", parseMsg); 

    /* Send retrieve message to the server */
    validatedWrite(sockfd, parseMsg, parseMsgLen, "parse write failure");
    free(parseMsg); 
}



/* Print the header content, start at the first '(' and end when this bracket is closed. 
    Return the number of characters printed. 
 */
int printHeaders(char *header, char *content, int contentLen) {
    // fprintf(stderr, "-start printing email-\n"); 
    printf("%s", header); 
    int printed = 0; 
    int bracket = 0; 
    int body = 0; 
    for (int ch=0; ch<contentLen; ch++) {
        fprintf(stderr, "(%c %d)", content[ch], content[ch]); 

        /* If we found the ending-bracket of the email, stop printing; 
            assuming there is ever going to be at most one message as discussed in ed#856 */
        if (content[ch] == ')') {
            bracket--; 
            if (bracket == 0) {
                /* Finished reading the message */
                break; 
            }
        }

        /* If we are in the middle of the bracket, print the content after the header */
        if (bracket > 0) {
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
    if (printed > 0) {
        printf("\n"); 
    }

    return printed; 
}