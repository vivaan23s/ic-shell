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
#define MAX_JOBS 10

typedef struct job {
    pid_t pid;
    char* command;
    int status;
} job;

job* jobs[MAX_JOBS];
int num_jobs = 0;

volatile pid_t current_foreground_process = -1;
int previous_exit_status = 0;

char* input_filename = NULL;
char* output_filename = NULL;

#define RUNNING 1
#define TERMINATED 2

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
        job* new_job = malloc(sizeof(job));
        new_job->pid = child_pid;
        new_job->command = command;
        new_job->status = RUNNING;
        jobs[num_jobs] = new_job;
        num_jobs++;
    }
    return 0;
}

void jobs_command() {
    for (int i = 0; i < num_jobs; i++) {
        job* job = jobs[i];
        printf("[%d] %d %s\n", i + 1, job->pid, job->command);
    }
}

void fg_command(int job_id) {
    if (job_id > num_jobs || job_id < 1) {
        printf("Invalid job ID\n");
        return;
    }

    job* job = jobs[job_id - 1];

    if (job->status == TERMINATED) {
        printf("Job %d has already terminated\n", job_id);
        return;
    }

    kill(job->pid, SIGCONT);
    current_foreground_process = job->pid;

    int status;
    wait(&status);
    previous_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
    current_foreground_process = -1;
    job->status = TERMINATED;
}

void bg_command(int job_id) {
    if (job_id > num_jobs || job_id < 1) {
        printf("Invalid job ID\n");
        return;
    }

    job* job = jobs[job_id - 1];

    if (job->status == TERMINATED) {
        printf("Job %d has already terminated\n", job_id);
        return;
    }

    kill(job->pid, SIGCONT);
    job->status = RUNNING;
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

        if (strchr(buffer, '>')) {
            char* previous = strtok(buffer, ">");
            char* later = strtok(NULL, ">");
            if (previous != NULL && later != NULL) {
                char cmd_copy[MAX_CMD_BUFFER];
                strncpy(cmd_copy, previous, sizeof(cmd_copy) - 1);
                cmd_copy[sizeof(cmd_copy) - 1] = '\0';
                char* final = strtok(later, " \t\n");
                if (final != NULL) {
                    output_filename = final;
                }
            }
        }

        if (strchr(buffer, '<')) {
            char* previous = strtok(buffer, "<");
            char* later = strtok(NULL, "<");
            if (previous != NULL && later != NULL) {
                char cmd_copy[MAX_CMD_BUFFER];
                strncpy(cmd_copy, previous, sizeof(cmd_copy) - 1);
                cmd_copy[sizeof(cmd_copy) - 1] = '\0';
                char* final = strtok(later, " \t\n");
                if (final != NULL) {
                    if (access(final, F_OK) != -1) {
                        input_filename = final;
                    } else {
                        printf("Error: Input file '%s' does not exist\n", final);
                    }
                } else {
                    printf("Error: Input file not specified\n");
                }
            }
        }

        if (!strcmp(buffer, "!!")) {
            if (strlen(last_cmd)) {
                printf("%s\n", last_cmd);
                strcpy(buffer, last_cmd);
                FILE* fp = popen(buffer, "r");
                if (fp) {
                    fgets(cmd_out, MAX_CMD_BUFFER, fp);
                    pclose(fp);
                }
                if (strlen(cmd_out)) printf("%s", cmd_out);
            } else continue;
        } else if (strncmp(buffer, "echo ", 5) == 0) {
            printf("%s\n", buffer + 5);
            strcpy(last_cmd, buffer + 5);
        } else if (strncmp(buffer, "exit", 4) == 0) {
            char* exit_code = buffer + 4;
            if (strlen(exit_code) > 0) {
                char* endptr;
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
        } else if (strcmp(buffer, "jobs") == 0) {
            jobs_command();
        } else if (strncmp(buffer, "fg ", 3) == 0) {
            char* job_id_str = buffer + 3;
            char* endptr;
            int job_id = strtol(job_id_str, &endptr, 10);
            if (*endptr == '\0') {
                fg_command(job_id);
            } else {
                printf("Invalid job ID\n");
            }
        } else if (strncmp(buffer, "bg ", 3) == 0) {
            char* job_id_str = buffer + 3;
            char* endptr;
            int job_id = strtol(job_id_str, &endptr, 10);
            if (*endptr == '\0') {
                bg_command(job_id);
            } else {
                printf("Invalid job ID\n");
            }
        } else {
            running_program(buffer);
        }
    }

    return 0;
}
