#include <stdio.h>
#include <stdlib.h> 
#include "util.c"
#include <ctype.h>

#define MIME_VERSION_HEADER "MIME-Version: 1.0"
#define CONTENT_TYPE_HEADER "Content-Type: multipart/alternative;"
#define BOUNDARY_PRE "boundary="
#define CONTENT_TYPE "Content-Type: text/plain;"
#define CHARSET "charset=UTF-8"
#define CONTENT_TRANSFER_ENCODING_PRE "Content-Transfer-Encoding:"
const char* CONTENT_TRANSFER_ENCODING[] = {"quoted-printable", "7bit", "8bit"};


void Mime(int sockfd, char* messageNum);
char* writeMimeCmd(int sockfd, char *messageNum);
int readBoundaryValue(int sockfd, char* content, char* boundary, int* capacity, char* mimeMsg);
int readAndStoreUntilTaga(int sockfd, char** content, int* capacity, char* mimeMsg, int* boundaryLen);
void writeRetrieveCmdMime(int sockfd, char *messageNum);


void Mime(int sockfd, char *messageNum) {
    char* mimeMsg = writeMimeCmd(sockfd, messageNum); 
    writeRetrieveCmd(sockfd, messageNum); 
    
    int boundaryLen = 0;
    int capacity_content = 1024; 
    char *content = malloc(capacity_content); 
    int contentLen = readAndStoreUntilTaga(sockfd, &content, &capacity_content, mimeMsg, &boundaryLen);
    // int boundaryLen = readBoundaryValue(sockfd, content, boundary, &capacity_boundary, mimeMsg);
    
    // for (int t=0; t<contentLen; t++) {
    //     fprintf(stdout, "%c", content[t]); 
    // }
    // fprintf(stderr, "- contentLen = %d\n", contentLen); 

    int dashN = 0;
    for (int i = 0; i < contentLen-boundaryLen-2; i++) {
        if (content[i] == '\r' && dashN < 2) {
            dashN++;
            i+=1;
            fprintf(stderr, "%d %d\n", content[i], content[i+1]);
            continue;
        }
        printf("%c", content[i]);
    }
    printf("\n");
    // printEmail(content, contentLen);

    free(content); 
} 




char* writeMimeCmd(int sockfd, char *messageNum) {
    /* Construct mime message */
    int mimeMsgLen = strlen(MIME_VERSION_HEADER) + strlen(CRLF) + strlen(CONTENT_TYPE_HEADER) + strlen(CRLF) + strlen(SP) + strlen(BOUNDARY_PRE); 
    char *mimeMsg = malloc(mimeMsgLen+1); assert(mimeMsg);    // need to free
    strcpy(mimeMsg, MIME_VERSION_HEADER); 
    strcat(mimeMsg, CRLF);
    strcat(mimeMsg, CONTENT_TYPE_HEADER); 
    strcat(mimeMsg, CRLF);
    strcat(mimeMsg, SP);
    strcat(mimeMsg, BOUNDARY_PRE);
    fprintf(stderr, "- mimeMsg = %s\n", mimeMsg); 
    return mimeMsg;
}


int readAndStoreUntilTaga(int sockfd, char **content, int *capacity, char* mimeMsg, int* boundaryLen) {
    char response[DEFAULT_BUFFER_SIZE];
    int tagged = 0;     // a flag either 0 or 1 to see if we successfully read the tag of the message
    int success = 0;    // a flag either 0 or 1 to see if we successfully read the server response
    int bracket = 0;    // count to see if all brackets are enclosed 
    int isLineFinished = LINE_FINISHED; 

    int capacity_boundary = 1025;
    char* boundary = malloc(capacity_boundary);
    int contentTransferEncodingOk = 0;



    char *target = RETRIEVE_TAG_SP; 
    int targetLen = strlen(target); 
    char* string_read = malloc(32*sizeof(char));
    int cmp = 0;        // the index of the target message that we need to compare 
    int i = 0;          // the index of the response from the server  
    int n;              // the length of the response from the server 
    int contentLen = 0;     // the number of character read into `content`
    int cmpMime = 0;
    int mimeFound = 0;
    int mimeMsgLen = strlen(mimeMsg);
    int boundaryCmp = 0;
    int CTECmp = 0;
    int CTELen = strlen(CONTENT_TRANSFER_ENCODING_PRE);
    int bodyCTCmp = 0;
    int bodyCTLen = strlen(CONTENT_TYPE);
    int nextBoundaryCmp = 0;
    int next_boundary_delimiter = 0;
    int boundary_delimiter = 0;
    int CTE_delimiter = 0;
    int body_CT_delimiter = 0;
    int charsetFound = 0;
    int charsetLen = strlen(CHARSET);
    int charsetCmp = 0;
    int iCT, iCTE;
    char* contentTransferEncodingType;



    /* Keep reading until the tag is read */
    while (!success) {
        
        n = validatedRead(sockfd, response, DEFAULT_BUFFER_SIZE, "Failure in reading RETRIEVE responses from the server\n"); 
        // fprintf(stderr, "- response(%d) = %s\n", n, response);
        // printf("%s", response);

        if (!tagged) {
            for (i=0; i<n; i++) {
                // fprintf(stderr, "%d %d %d %d %d %d\n", mimeFound, boundary_delimiter, CTE_delimiter, body_CT_delimiter, charsetFound, tagged);
                // fprintf(stderr, "cTE: %d\n", contentTransferEncodingOk);
                if (mimeFound && !next_boundary_delimiter) {
                    char* template_boundary_delimiter = malloc(strlen(CRLF) + strlen("--") + strlen(boundary) + strlen(CRLF) + 1);
                    strcpy(template_boundary_delimiter, CRLF);
                    // strcat(template_boundary_delimiter, SP);
                    strcat(template_boundary_delimiter, "--");
                    strcat(template_boundary_delimiter, boundary);
                    strcat(template_boundary_delimiter, CRLF);
                    *boundaryLen = strlen(template_boundary_delimiter);
                    // fprintf(stderr, "%s\n", template_boundary_delimiter);

                    while (!boundary_delimiter) {
                        if (boundaryCmp >= *boundaryLen) {
                            boundary_delimiter = 1;
                            // fprintf(stderr, "Success boundary\n");
                            break;
                        }
                        if (response[i] == template_boundary_delimiter[boundaryCmp]) {
                            
                            boundaryCmp += 1;

                        }
                        else {
                            
                            /* Find the next CRLF */
                            while (i < n-1 && (response[i] != CRLF[0] || response[i+1] != CRLF[1])) {
                                // fprintf(stderr, "response: %c   ", response[i]);
                                i++;
                            }
                            i++;
                            boundaryCmp = 2;
                            //fprintf(stderr, "match: %c\n", response[i+1]);
                        }
                        if (i >= n) {
                            break;
                        }
                        i++;
                    }

                    // remember where i is when boundary value finishes so I can use it later for content type matching
                    int original_i = i;

                    if (!CTE_delimiter || !body_CT_delimiter || !contentTransferEncodingOk) {

                        
                        // this to match CTE_PRE
                        
                        while (!CTE_delimiter) {
                            if (CTECmp >= CTELen) {
                                CTE_delimiter = 1;
                                iCTE = i;
                                // fprintf(stderr, "Success CTE\n");
                                break;
                            }
                            if (response[i] == CONTENT_TRANSFER_ENCODING_PRE[CTECmp]) {
                                
                                CTECmp += 1;

                            }
                            else {
                                
                                /* Find the next CRLF */
                                while (i < n-1 && (response[i] != CRLF[0] || response[i+1] != CRLF[1])) {
                                    // fprintf(stderr, "response: %c   ", response[i]);
                                    i++;
                                    
                                }
                                i++;
                                CTECmp = 0;
                                //fprintf(stderr, "match: %c\n", response[i+1]);
                            }
                            if (i >= n) {
                                break;
                            }
                            i++;
                        }

                        i++;
                        // initial copying into string_read
                        char s[2]; 
                        s[0] = response[i]; 
                        s[1] = '\0'; 
                        strcpy(string_read, s);
                        i++;
                        while (i < n-1 && (response[i] != CRLF[0] || response[i+1] != CRLF[1])) {
                            // fprintf(stderr, "response: %c   ", response[i]);
                            strncat(string_read, response+i, 1);
                            i++;
                        }

                        // compare read string with the strings in array TRANSFER_ENCODING
                        for (int j = 0; j < 3; j++) {
                            if (strcmp(string_read, CONTENT_TRANSFER_ENCODING[j]) == 0) {
                                contentTransferEncodingOk = 1;
                                contentTransferEncodingType = malloc(strlen(CONTENT_TRANSFER_ENCODING[j]));
                                strcpy(contentTransferEncodingType, CONTENT_TRANSFER_ENCODING[j]);
                                break;
                            }
                        }
                        
                        // reload i into original_i to search for body_content_type
                        i = original_i;
                        while (!body_CT_delimiter) {
                            if (bodyCTCmp >= bodyCTLen) {
                                body_CT_delimiter = 1;
                                iCT = i;
                                // fprintf(stderr, "Success CTE\n");
                                break;
                            }
                            if (tolower(response[i]) == tolower(CONTENT_TYPE[bodyCTCmp])) {
                                
                                bodyCTCmp += 1;

                            }
                            else {
                                /* Find the next CRLF */
                                while (i < n-1 && (response[i] != CRLF[0] || response[i+1] != CRLF[1])) {
                                    // fprintf(stderr, "response: %c   ", response[i]);
                                    i++;
                                    
                                }
                                i++;
                                bodyCTCmp = 0;
                                //fprintf(stderr, "match: %c\n", response[i+1]);
                            }
                            if (i >= n) {
                                break;
                            }
                            i++;
                        }
                        while (!charsetFound) {
                            if (i >= n) {
                                break;
                            }
                            if (charsetCmp >= charsetLen) {
                                charsetFound = 1;
                                // fprintf(stderr, "Success CTE\n");
                                break;
                            }
                            if (tolower(response[i]) == tolower(CHARSET[charsetCmp])) {
                                charsetCmp += 1;
                            }
                            else {
                                charsetCmp = 0;
                                //fprintf(stderr, "match: %c\n", response[i+1]);
                            }
                            
                            i++;
                        }
                        i = max(iCT+1+strlen(CHARSET), iCTE+1+strlen(contentTransferEncodingType));
                    }
                    
            
                    // intialise content
                    if (contentLen == 0) {
                        char c[2]; 
                        c[0] = response[i]; 
                        c[1] = '\0'; 
                        strcpy(*content, c);
                        contentLen += 1;
                        i++;
                    }

                    while (!next_boundary_delimiter) {
                        if (contentLen >= *capacity) {
                            // fprintf(stderr, "-realloc content-\n"); 
                            *capacity *= 2; 
                            *content = realloc(*content, *capacity); 
                        }

                        if (i >= n) {
                            break;
                        }
                        strncat(*content, response+i, 1);
                        contentLen += 1;
                        for (int k = contentLen-*boundaryLen; k < contentLen; k++) {
                            if (nextBoundaryCmp >= *boundaryLen) {
                                // for (int l = 0; l < )
                                next_boundary_delimiter = 1;
                                break;
                            }
                            if (tolower((*content)[k]) == tolower(template_boundary_delimiter[nextBoundaryCmp])) {
                                nextBoundaryCmp++;
                                // fprintf(stderr, "%s\n", *content);
                            }
                            else {
                                nextBoundaryCmp = 0;
                                break;
                            }
                        }
                        
                        i++;
                    }
                    
                }
                // fprintf(stderr, "checking %d -> %c\n", i, response[i]); 
                /* If read the tag, update flag and break */

                if (cmp >= targetLen) {
                    tagged = 1; 
                    targetLen = strlen(OK_MSG_SP); 
                }

                if (tagged && next_boundary_delimiter) {
                    fprintf(stderr, "BROEKNISFINIFDNIFNINF\n");
                    break;
                }

                

                // check if it's mime
                if (tolower(response[i]) == tolower(mimeMsg[cmpMime])) {
                    cmpMime += 1;
                    if (cmpMime >= mimeMsgLen) {
                        mimeFound = 1;
                        i++;
                        while ((response[i] != '\r' && response[i+1] != '\n')){
                            if (response[i] == '"' || response[i] == ';') {
                                i++;
                                continue;
                            }
                            /* Store the content of the email */
                            if (*boundaryLen >= capacity_boundary) {
                                // fprintf(stderr, "-realloc content-\n"); 
                                capacity_boundary *= 2; 
                                boundary = realloc(boundary, *capacity); 
                            }
                            (boundary)[*boundaryLen] = response[i];
                            (*boundaryLen)++;
                            i++;
                        }
                    }
                }
                else {
                    cmpMime = 0;
                }
                

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
                        if (tolower(response[i]) == tolower(target[cmp])) {
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

        /* Found the response tag, get the boundary-parameter-value and compare content-type and content-type encoding */
        if (tagged) {
            for (cmp=0; i+cmp<n; cmp++) {
                if (cmp >= targetLen && mimeFound) {
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

    // create template boundary delimiter
    

    // for (int t=0; t<contentLen; t++) {
    //     fprintf(stdout, "%c", (*content)[t]); 
    // }

    return contentLen; 
}


