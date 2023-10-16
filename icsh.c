/* ICCS227: Project 1: icsh
 * Name:Varaaphon Chotthanawathit 
 * StudentID:u6481100
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CMD_BUFFER 255

volatile pid_t current_foreground_process = -1;
int previous_exit_status = 0;

void signal_handler(int signal) {
    if (current_foreground_process > 0) {
        switch (signal) {
            case SIGTSTP:
                
                kill(current_foreground_process, SIGTSTP);
                 current_foreground_process = -1;
                break;
            case SIGINT:
                
                kill(current_foreground_process, SIGINT);
                current_foreground_process = -1;
                break;
            default:
                break;
        }
    }
}

int script(char* filename) {
    char buffer[MAX_CMD_BUFFER];
    FILE* script_file = fopen(filename, "r");
    if (script_file == NULL) {
        printf("Error: File does not exist  '%s'\n", filename);
        return 1;
    }
    int exit_code = 0;
    char last_echo[MAX_CMD_BUFFER] = "";

    while (fgets(buffer, MAX_CMD_BUFFER, script_file) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (strlen(buffer) == 0) continue;
        if (strncmp(buffer, "exit ", 4) == 0) {
            char* exit_code_str = buffer + 4;
            char* endptr;
            long exit_value = strtol(exit_code_str, &endptr, 10);
            if (*endptr == '\0') {
                exit_code = (int)exit_value;
                break;
            }
        } else if (strcmp(buffer, "!!") == 0) {
            if (strlen(last_echo) > 0) {
                printf("%s\n", last_echo);
            } else {
                printf("bad command\n");
            }
        } else if (strncmp(buffer, "echo ", 5) == 0) {
            printf("%s\n", buffer + 5);
            strcpy(last_echo, buffer + 5);
        } else {
            printf("bad command\n");
        }
    }
    fclose(script_file);
    return exit_code;
}

int running_program(char* command) {
    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork failed");
        return 1; 
    } else if (child_pid == 0) {
        char* token;
        char* args[MAX_CMD_BUFFER];
        int i = 0;
        token = strtok(command, " ");
        while (token != NULL) {
            args[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        args[i] = NULL;
        
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    } else {
        current_foreground_process = child_pid; 
        int status;
        wait(&status);
        previous_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
        current_foreground_process = -1; 
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("Child exit status = %d\n", exit_code);
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        int exit_code = script(argv[1]);
        printf("$ echo $?\n%d\n", exit_code);
        return exit_code;
    }
    
    char buffer[MAX_CMD_BUFFER], last_cmd[MAX_CMD_BUFFER] = "", cmd_out[MAX_CMD_BUFFER] = "";
   
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    printf("Starting IC shell\n");
    while (1) {
        printf("icsh $ ");
        fgets(buffer, MAX_CMD_BUFFER, stdin);
        buffer[strcspn(buffer, "\n")] = 0;
        if (!strlen(buffer)) continue;
        if (!strcmp(buffer, "!!")) {
            if (strlen(last_cmd)) {
                printf("%s\n", last_cmd);
                strcpy(buffer, last_cmd);
                FILE *fp = popen(buffer, "r");
                if (fp) {
                    fgets(cmd_out, MAX_CMD_BUFFER, fp);
                    pclose(fp);
                }
                if (strlen(cmd_out)) printf("%s", cmd_out);
            } else continue;
        } else if (strncmp(buffer, "echo ", 5) == 0) {
            printf("%s\n", buffer + 5);
            strcpy(last_cmd, buffer);
        } else if (strncmp(buffer, "exit", 4) == 0) {
            char *exit_code = buffer + 4;
            if (strlen(exit_code) > 0) {
                char *endptr;
                long exit_value = strtol(exit_code, &endptr, 10);
                if (*endptr == '\0') {
                    printf("Goodbye :)\n");
                    exit(exit_value >= 0 && exit_value <= 255 ? (int)exit_value : 0);
                } else {
                    printf("Invalid exit code\n");
                }
            } else {
                printf("Goodbye :)\n");
                exit(0);
            }
        } else if (strcmp(buffer, "echo $?") == 0) {
            printf("%d\n", previous_exit_status);
        } else {
            running_program(buffer);
        }
    }

    return 0;
}