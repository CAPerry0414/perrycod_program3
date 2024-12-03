#define  _POSIX_C_SOURCE  200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_CHARS 2048
#define MAX_ARGS 512
#define MAX_PROCESSES 64

//For use with handle_SIGTSTP and background processes
int foregroundOnly = 0;
int foreground = 1;

//Function to replace a substring with a specified value
char* replaceSubstring(char oldString[], char newSubstring[], int len, int index) {
    char *substr1 = malloc(sizeof(char) * MAX_CHARS);
    char *substr2 = malloc(sizeof(char) * MAX_CHARS);
    char *replacementString = malloc(sizeof(char) * MAX_CHARS);

    //Error checking
    if (newSubstring == "") {
        return oldString;
    }

    //Create the first and second substrings
    for (int i = 0; i < index; i++) {
        substr1[i] = oldString[i];
    }

    for (int i = (index + len), j = 0; i < strlen(oldString); i++, j++) {
        substr2[j] = oldString[i]; 
    }

    //Append the first substring, new substring, and the second substring to the replacement
    strcpy(replacementString, substr1);
    strcat(replacementString, newSubstring);
    strcat(replacementString, substr2);

    return replacementString;

}

//Signal handler for SIGTSTP
void handle_SIGTSTP(int signo) {
    //Changes the foregroundOnly variable and prints a message to the console
    if (foregroundOnly == 0) {
        char *message = "\nEntering foreground-only mode (& is now ignored)\n: ";
        write(STDOUT_FILENO, message, 52);
        foregroundOnly = 1;
    }
    else {
        char *message = "\nExiting foreground-only mode\n: ";
        write(STDOUT_FILENO, message, 32);
        foregroundOnly = 0;
    }
}

int main() {
    //Values for status
    int exitVal = 0;
    int exited = 1;
    int sigVal = 0;

    //Array of background processes for later
    int backgroundProcesses[MAX_PROCESSES];
    for (int i = 0; i < MAX_PROCESSES; i++) {
        backgroundProcesses[i] = -1;
    }

    //Creating sigactions
    struct sigaction ignore_action = {0}, SIGTSTP_action = {0};
    struct sigaction SIGINT_action = {0};

    //Defining sigactions
    ignore_action.sa_handler = SIG_IGN;
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    SIGINT_action.sa_handler = SIG_DFL;

    sigfillset(&ignore_action.sa_mask);
    sigfillset(&SIGTSTP_action.sa_mask);
    sigfillset(&SIGINT_action.sa_mask);

    ignore_action.sa_flags = 0;
    SIGTSTP_action.sa_flags = SA_RESTART;
    SIGINT_action.sa_flags = 0;

    //Colon to prompt the command line.
    while (1) {
        //Parent Process Ignores SIGINT
        sigaction(SIGINT, &ignore_action, NULL);

        //Parent process runs the signal handler for SIGTSTP
        sigaction(SIGTSTP, &SIGTSTP_action, NULL);

        //Getting the pid for later
        pid_t currPid = getpid();
        //Registers an input
        char input[MAX_CHARS];
        printf(": ");
        fgets(input, MAX_CHARS, stdin);

        //Remove the trailing newline char
        input[strcspn(input, "\n")] = '\0';

        //Checking for the ampersand in the command
        char *checkAmpersand = strchr(input, '&');
        if (checkAmpersand != NULL && !foregroundOnly) {
            foreground = 0;
        }

        //Ends the shell and terminates all processes. #4-1
        int check = strncmp(input, "exit", 4);
        if (check == 0) {
            exit(0);
            break;
        }

        else {
            //Comment and blank line checking. #2
            if (strncmp(input, "#", 1) == 0 || strcmp(input, "") == 0) {
                continue;
            }

            //$$ Checking. #3
            //Use a while loop to check every character in the input.
            for (int i = 0, j = 1; j < strlen(input); i++, j++) {
                char a = input[i];
                char b = input[j];
                if (a == '$' && b == '$') {
                    //Turn the pid into a string
                    char strPid[50];
                    sprintf(strPid, "%d", currPid);

                    //Replace the substring of "$$" with the pid, starting at y and ending with j
                    char *newString = replaceSubstring(input, strPid, 2, i);
                    strcpy(input, newString);

                    free(newString);
                    break;
                }
            }
            //For use with strtok_r
            char *saveptr;

            char *token = strtok_r(input, " ", &saveptr);
            char *command = malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(command, token);

            //cd built-in command. #4-2
            if (strncmp(command, "cd", 2) == 0) {
                token = strtok_r(NULL, " ", &saveptr);

                //If no directory is specified, change the cwd to home
                if (token == NULL) {
                    int dirChange = chdir(getenv("HOME"));
                }

                else {
                    //Create a token out of the specified path
                    char *path = malloc(sizeof(char) * (strlen(token) + 1));
                    strcpy(path, token);

                    //Change the directory to the specified path
                    int dirChange = chdir(path);
                    if (dirChange >= 0) {
                        printf("I am now in the directory %s\n", path);
                    }
                    else {
                        printf("Invalid path\n");
                    }
                    
                    free(path);
                    continue;
                }
            }

            //status built-in command. #4-3
            if (strcmp(command, "status") == 0) {
                //Prints different messages depending on if the process terminated normally
                if (exited) {
                    printf("exit value %d\n", exitVal);
                }
                else {
                    printf("terminated by signal %d\n", sigVal);
                }
                continue;
            }
            
            //Splitting the command line
            char *arguments[MAX_ARGS];

            //First element in the arguments array is the command
            int i = 0;
            arguments[i] = command;
            i++;

            int isInput;
            int isOutput;
            int isAmpersand;
            
            token = strtok_r(NULL, " ", &saveptr);
            if (token != NULL) {
                isInput = strcmp(token, "<");
                isOutput = strcmp(token, ">");
                isAmpersand = strcmp(token, "&");
            }
            //Ensuring that the token isn't the start of an input file or output file, and isn't an ampersand
            if (token != NULL && isInput && isOutput && isAmpersand) {
                //Iterate through the new token, and add every argument individually to the array
                while (token != NULL && isInput && isOutput && isAmpersand) {
                    arguments[i] = malloc(sizeof(char) * MAX_CHARS);
                    strcpy(arguments[i], token);
                    i++;
                    token = strtok_r(NULL, " ", &saveptr);

                    if (token != NULL) {
                        isInput = strcmp(token, "<");
                        isOutput = strcmp(token, ">");
                        isAmpersand = strcmp(token, "&");
                    } 
                }
            }

            //Last element is NULL
            arguments[i] = NULL;
            
            //Creating new processes. #5
            pid_t spawnpid = -5;
            int childStatus;
            
            spawnpid = fork();
            if (spawnpid == -1) {
                perror("fork() failed!!");
                continue;
            }

            //Setting up the backgroundProcesses array
            if (foreground == 0) {
                int i = 0;
                while (backgroundProcesses[i] > 0) {
                    i++;
                }
                backgroundProcesses[i] = spawnpid;

                if (spawnpid != 0) {
                    //Print the process pid
                    printf("background pid is %d\n", spawnpid);
                }
            }
                    

            if (spawnpid == 0) {
                //Ignoring SIGTSTP
                struct sigaction SIGTSTP_action_child = {0};
                SIGTSTP_action_child.sa_handler = SIG_IGN;
                sigaction(SIGTSTP, &SIGTSTP_action_child, NULL);

                //Checking for I/O redirection. #6
                //Input
                char inputPath[MAX_CHARS];


                if (token != NULL && !isInput) {
                    token = strtok_r(NULL, " ", &saveptr);
                    strcpy(inputPath, token);
                    //Creating a file descriptor for the input file
                    int inputFd = open(inputPath, O_RDONLY);

                    //Error checking
                    if (inputFd == -1) {
                        perror("open()");
                        exit(1);
                    }

                    //Redirecting stdin to inputFd
                    int newInput = dup2(inputFd, STDIN_FILENO);
                    
                    //Error checking again
                    if (newInput == -1) {
                        perror("dup2()");
                        exit(1);
                    }

                    token = strtok_r(NULL, " ", &saveptr);
                }

                //Redeclaring variables
                if (token != NULL) {
                    isOutput = strcmp(token, ">");
                    isAmpersand = strcmp(token, "&");
                }
                

                //Output
                char outputPath[MAX_CHARS];
                if (token != NULL && !isOutput) {
                    token = strtok_r(NULL, " ", &saveptr);
                    strcpy(outputPath, token);

                    //Creating a file descriptor for the output file
                    int outputFd = open(outputPath, O_WRONLY);

                    //Error checking
                    if (outputFd == -1) {
                        perror("open()");
                        exit(1);
                    }

                    //Redirecting stdout to outputFd
                    int newOutput = dup2(outputFd, STDOUT_FILENO);

                    //Error checking again
                    if (newOutput == -1) {
                        perror("dup2()");
                        exit(1);
                    }

                    token = strtok_r(NULL, " ", &saveptr);
                }

                //If the process is running in the background... 
                if (foreground == 0) {
                    //Check for I/O redirection
                    int defaultFd = open("/dev/null", O_RDWR);
                    //If there is no input path, redirect it to "/dev/null"
                    if (strlen(inputPath) == 0) {
                        int newInput = dup2(defaultFd, STDIN_FILENO);
                    }

                    //If there is no output path, redirect it to "/dev/null"
                    if (strlen(outputPath) == 0) {
                        int newOutput = dup2(defaultFd, STDOUT_FILENO);
                    }

                    //Ignore SIGINT
                    sigaction(SIGINT, &ignore_action, NULL);
                }

                if (foreground == 1) {
                    //Default SIGINT action
                    struct sigaction SIGINT_action_child = {0};
                    SIGINT_action_child.sa_handler = SIG_DFL;
                    sigaction(SIGINT, &SIGINT_action_child, NULL);
                }

                //Uses execv to run the specified command with the inputted arguments
                execvp(command, arguments);
                //Error checking 
                perror("execvp");
                exit(1);
            }

            else {
                //If the child process is in the foreground, just wait for it to finish its execution
                if (foreground > 0) {
                    pid_t childPid = waitpid(spawnpid, &childStatus, 0);
                }
                
                //If the process is in the background. #7
                else {
                    pid_t childPid = waitpid(spawnpid, &childStatus, WNOHANG);
                    foreground = 1;
                    continue;
                }
                
                //If the child process terminated with an error, change the status exit value to 1
                if (WEXITSTATUS(childStatus) == 1) {
                    exitVal = WEXITSTATUS(childStatus);
                }

                //Check to see if the child process was terminated by some signal
                if (WIFSIGNALED(childStatus)) {
                    int signalVal = WTERMSIG(childStatus);   
                    printf("terminated by signal %d\n", signalVal);
                    sigVal = signalVal;
                    exited = 0;
                }

                //Iterate through the pids in backgroundProcesses
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (backgroundProcesses[i] > 0) {
                        pid_t tempPid = waitpid(backgroundProcesses[i], &childStatus, WNOHANG);
                        //Check to see if they're done, and return an exit statement if they are
                        //Checking exit statuses and signals
                        if (tempPid > 0) {
                            if (WIFEXITED(childStatus)) {
                                exitVal = WEXITSTATUS(childStatus);
                                //Changing the exit status if the child terminated with an error
                                if (exitVal != 0) 
                                    exitVal = 1;
                            
                                printf("background pid %d is done: exit value %d\n", backgroundProcesses[i], exitVal);
                                backgroundProcesses[i] = -1;
                            }

                            if (WIFSIGNALED(childStatus)) {
                                int exitSig = WTERMSIG(childStatus);
                                printf("background pid %d is done: terminated by signal %d\n", backgroundProcesses[i], exitSig);

                                backgroundProcesses[i] = -1;
                            }
                        }
                        
                    }
                }
            }
            continue;
        }
    }
    return 0;
}