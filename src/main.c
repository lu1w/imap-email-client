#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cmd_retrieve.c"
#include "cmd_parse.c"
#include "cmd_list.c"
#include "util.c"

#define RETRIEVE "retrieve"
#define PARSE "parse"
#define LIST "list"
#define DEFAULT_FOLDER "INBOX"
#define DEFAULT_MSG_NUM "*"

#define PORT "143"
#define LOGIN_TAG_SP "A00login "
#define LOGIN_MSG_SP "LOGIN "
#define SELECT_FOLDER_TAG_SP "A00select "
#define SELECT_FOLDER_MSG_SP "SELECT "

#define LOGIN_FAILURE "Login failure\n"
#define FOLDER_NOT_FOUND "Folder not found\n"

/* Validation of input */
void validateMessageNum(char *messageNum); 

/* Other functions */
int connectSocket(char *serverName); 
void login(int sockfd, char *username, char *password); 
void selectFolder(int sockfd, char *folder); 
void validateResponse(int sockfd, char *tag, char *errorMsg);


/* Entry point of the program */
int main(int argc, char **argv) {
    /* Process command line arguments */
    int i = 1; 
    char command = '\0'; 
    char *username = NULL; 
    char *password = NULL; 
    char *serverName = NULL; 
    char *folder = malloc((strlen(DEFAULT_FOLDER) + 1) * sizeof(char)); assert(folder); 
    char *messageNum = malloc((strlen(DEFAULT_MSG_NUM) + 1) * sizeof(char)); assert(messageNum); 

    /* Initialize default arguments */
    strcpy(folder, DEFAULT_FOLDER);
    strcpy(messageNum, DEFAULT_MSG_NUM); 

    /* Read arguments */
    while (i < argc) {
        if (strcmp(argv[i], "-u") == 0) { // Username 
            if (i+1 >= argc) { break; }
            username = malloc((strlen(argv[i+1]) + 1) * sizeof(char)); assert(username); 
            strcpy(username, argv[i+1]); 
            i += 2; 
        } else if (strcmp(argv[i], "-p") == 0) { // Password 
            if (i+1 >= argc) { break; }
            for (int j = 0; j < strlen(argv[i+1]); j++) {
                if (argv[i+1][j] == '\r' && argv[i+1][j+1] =='\n') {
                    printf("ERROR: missing argument\n"); 
                    exit(CMD_ARG_ERROR); 
                }
            }
            password = malloc((strlen(argv[i+1]) + 1) * sizeof(char)); assert(password); 
            strcpy(password, argv[i+1]); 
            i += 2; 
        } else if (strcmp(argv[i], "-f") == 0) { // Folder
            if (i+1 >= argc) { break; }
            /* Check of validity of input in the mail name */
            int hasSpace = 0; 
            for (int j=0; j<strlen(argv[i+1]); j++) {
                if (argv[i+1][j] == ' ') {
                    folder = (char* )realloc(folder, strlen(argv[i+1]) + 1 + 2); assert(folder); 
                    strcpy(folder, "\""); 
                    strcat(folder, argv[i+1]); 
                    strcat(folder, "\""); 
                    hasSpace = 1; 
                    break; 
                }
            }
            if (!hasSpace) {
                folder = (char* )realloc(folder, (strlen(argv[i+1]) + 1) * sizeof(char)); assert(folder); 
                strcpy(folder, argv[i+1]);
            }
            i += 2; 
        } else if (strcmp(argv[i], "-n") == 0) { // Message number 
            if (i+1 >= argc) { break; }
            messageNum = (char* )realloc(messageNum, (strlen(argv[i+1]) + 1) * sizeof(char)); assert(messageNum); 
            strcpy(messageNum, argv[i+1]); 
            validateMessageNum(messageNum); 
            i += 2; 
        } else { 
            if (strcmp(argv[i], "-t") != 0) { // Positional argument 
                if (!command) { // Command 
                    command = argv[i][0]; 
                } else { // Server name 
                    serverName = malloc((strlen(argv[i]) + 1) * sizeof(char)); assert(serverName); 
                    strcpy(serverName, argv[i]); 
                }
            }
            i += 1; 
        }
    }

    /* Check that all required argument are read */
    if (!username || !password || !serverName || !command) {
        printf("ERROR: missing argument\n"); 
        exit(CMD_ARG_ERROR); 
    }

    int sockfd = connectSocket(serverName); 

    /* Log in */
    login(sockfd, username, password); 

    /* Select a folder */
    selectFolder(sockfd, folder); 

    /* Run program */
    if (command == RETRIEVE[0]) {
        /* Retrieve the email */
        retrieve(sockfd, messageNum); 
    } else if (command == PARSE[0]) {
        /* Parse the email */
        parse(sockfd, messageNum); 
    } else if (command == LIST[0]) {
        /* List the subject lines of all the emails in the specified folder */
        list(sockfd); 
    }

    free(username); 
    free(password); 
    free(folder); 
    free(serverName); 

    return 0;
}

/* Create socket connection 
    Credit to The University of Melbourne COMP30023 2024 Sem1 Lab8 client.c. 
*/
int connectSocket(char *serverName) {
    int sockfd, s;
	struct addrinfo hints, *servinfo, *rp;

    /* Create address, try IPv6 first */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
    s = getaddrinfo(serverName, PORT, &hints, &servinfo); // serverName = "unimelb-comp30023-2024.cloud.edu.au"

    /* If failure connecting to IPv6 socket, try IPv4 */
    if (s != 0) {
        hints.ai_family = AF_INET;
        s = getaddrinfo(serverName, PORT, &hints, &servinfo);

        /* If failure to connect to IPv4 socket also, exit with failure */
        if (s != 0) {
            exit(CONNECTION_ERROR); 
        }
    }

    /* Connect to the first valid result */
    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1)
			continue; // invalid 
		if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; // success
		close(sockfd);
	}
    
	if (rp == NULL) {
		printf("Connection failure\n");
        exit(CONNECTION_ERROR);
	}
    freeaddrinfo(servinfo);

    return sockfd; 
}


/* Login the system using `username` and `password`. */
void login(int sockfd, char *username, char *password) {

    /* Send login message to the server */
    int loginMsgLen = strlen(LOGIN_TAG_SP) + strlen(LOGIN_MSG_SP) + strlen(username) + strlen(SP) + strlen(password) + strlen(CRLF);
    char *loginMsg = malloc(loginMsgLen + 1); 
    assert(loginMsg); 
    strcpy(loginMsg, LOGIN_TAG_SP); 
    strcat(loginMsg, LOGIN_MSG_SP); 
    strcat(loginMsg, username); 
    strcat(loginMsg, SP); 
    strcat(loginMsg, password); 
    strcat(loginMsg, CRLF); 
    
    validatedWrite(sockfd, loginMsg, loginMsgLen, "Failure in sending LOGIN command to the server\n");
    free(loginMsg); 

    /* Check if the login is completed successfully */
    validateResponse(sockfd, LOGIN_TAG_SP, LOGIN_FAILURE);


    /* RFC Note: 
    continue-req    = "+" SP (resp-text / base64) CRLF
    response        = *(continue-req / response-data) response-done
    response-data   = "*" SP (resp-cond-state / resp-cond-bye / mailbox-data / message-data / capability-data) CRLF
    response-done   = response-tagged / response-fatal
    response-tagged = tag SP resp-cond-state CRLF
    resp-cond-state = ("OK" / "NO" / "BAD") SP resp-text; Status condition
    response-fatal  = "*" SP resp-cond-bye CRLF; Server closes connection immediately
    */

    /* Read responce from the server until the response is completed 
        Possible Responce:    
            OK - login completed, now in authenticated state
            NO - login failure: user name or password rejected
            BAD - command unknown or arguments invalid
        (Reference to: RFC3501 LOGIN Command, https://datatracker.ietf.org/doc/html/rfc3501#autoid-49)
    */
    
      
}

/* Select a folder from the email server. */
void selectFolder(int sockfd, char *folder) {
    /* Construct message */
    char* folderSelectionMsg = malloc(strlen(SELECT_FOLDER_TAG_SP) + strlen(SELECT_FOLDER_MSG_SP) + strlen(folder) + strlen(CRLF) + 1); 
    assert(folderSelectionMsg); 
    strcpy(folderSelectionMsg, SELECT_FOLDER_TAG_SP); 
    strcat(folderSelectionMsg, SELECT_FOLDER_MSG_SP); 
    strcat(folderSelectionMsg, folder); 
    strcat(folderSelectionMsg, CRLF); 

    /* Send message to server */
    validatedWrite(sockfd, folderSelectionMsg, strlen(folderSelectionMsg), "Failure in sending SELECT command to the server\n");
    free(folderSelectionMsg); 

    /* Validate response */
    validateResponse(sockfd, SELECT_FOLDER_TAG_SP, FOLDER_NOT_FOUND);
}


void validateResponse(int sockfd, char *tag, char *errorMsg) {
    char response[1024];
    int targetLen = strlen(tag); 
    int cmp = 0; 
    int n, i;
    int success = 0; 
    int tagged = 0; 
    while (!success) {
        n = validatedRead(sockfd, response, 1024, errorMsg); 

        /* Try to read the tag */
        if (!tagged) {
            for (i=0; i < n; i++) {

                /* If read an success message, update flag and break */
                if (cmp >= targetLen) {
                    tagged = 1; 
                    targetLen = strlen(OK_MSG_SP); 
                    break; 
                }

                /* Compare the response and the success message */
                if (response[i] != tag[cmp]) {
                    cmp = 0; 
                    /* Find the next CRLF */
                    while (i < n-1 && (response[i] != CRLF[0] || response[i+1] != CRLF[1])) {
                        i++;
                    }
                    i++;
                } else {
                    cmp++; 
                }
            }
        }

        if (tagged) {
            for (cmp=0; i+cmp<n; cmp++) {
                if (cmp >= targetLen) {
                    /* OK - Successfully fetched */
                    success = 1; 
                    break; 
                }
                if (response[i+cmp] != OK_MSG_SP[cmp]) {
                    /* Failure to perform the command, exit */
                    printf("%s", errorMsg); 
                    exit(SERVER_ERROR); 
                }
            }
        }
        
    }
}

/* Validate that input is in correct format */
void validateMessageNum(char *messageNum) {
    /* Make sure the message number is a numeric value only */
    int i = 0; 
    while (messageNum[i] != '\0') {
        if (messageNum[i] <= '0' || messageNum[i] > '9') {
            fprintf(stderr, "ERROR: Invalid message number\n"); 
            exit(CMD_ARG_ERROR); 
        }
        i++; 
    }
}