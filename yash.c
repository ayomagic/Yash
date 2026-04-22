#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define STOPPED "Stopped"
#define DONE "Done"
#define RUNNING "Running"
#define TRUE 1
#define FALSE 0
#define BACKGROUND 1
#define NO_BACKGROUND 0

typedef struct Job {
    char *name;
    int jobID;
    pid_t pID;
    char *status; 
    int background;
    struct Job *next;
}Job;

void add(char *NAME, pid_t PID, char *STAT, int BACK);
void removenode(pid_t PID);
void printList();
void handler(int sig);
int updatenode(pid_t PID, char *STATUS);
int found(pid_t PID);
void freelist();
void fg_call();
void bg_call();

volatile pid_t parentPID;
Job *head = NULL;
char *jname = "";
int pipe_i;
pid_t pipedGID;

int main(void) {
    /* signal stuff... */
    parentPID = getpid();
    signal(SIGCHLD, handler);
    signal(SIGINT, handler);
    signal(SIGTSTP, handler);
    signal(SIGCONT, SIG_DFL);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    int background = FALSE;

    while (TRUE) {
        /* Initialization stuff... */
        pipe_i = 0;
        background = FALSE;
        char *cmd = NULL, *token = NULL, *cmd_cpy = NULL;
        char *delim = " \n";
        size_t n = 0;
        int argc = 0, i = 0;
        int in = 0, out = 0, err = 0;
        int pipe_in = 0, pipe_out = 0, pipe_err = 0;
        int ptr_in = 0, ptr_out = 0, ptr_err = 0;
        int ptr_pipe_in = 0, ptr_pipe_out = 0, ptr_pipe_err = 0;
        int pipefd[2], status, done = 0;
        int fg = 0, bg = 0;

        char **argv = NULL;
        printf("# ");
        if (getline(&cmd, &n, stdin) == -1) {
            freelist();
            return -1;
        }

        cmd_cpy = strdup(cmd);
        token = strtok_r(cmd, delim, &cmd);
        if (token == NULL) 
            continue;

        while (token) {
            token = strtok_r(NULL, delim, &cmd);
            argc++;
        }
        argv = malloc(sizeof(char *) * argc);
        int stop_token = argc, piped_stop_token = argc;
        token = strtok_r(cmd_cpy, delim, &cmd_cpy);
        while (token) {
            /* check for special commands */
            if (strcmp(token, "<") == 0 && i != 0) {
                if (pipe_i > 0) {
                    pipe_in = 1;
                    ptr_pipe_in = i + 1;
                    if (piped_stop_token > i) piped_stop_token = i;
                } else {
                    in = 1;
                    ptr_in = i + 1;
                    if (stop_token > i) stop_token = i;
                }
            } else if (strcmp(token, ">") == 0 && i != 0) {
                if (pipe_i > 0) {
                    pipe_out = 1;
                    ptr_pipe_out = i + 1;
                    if (piped_stop_token > i) piped_stop_token = i;
                } else {
                    out = 1;
                    ptr_out = i + 1;
                    if (stop_token > i) stop_token = i;
                }
            } else if (strcmp(token, "2>") == 0 && i != 0) {
                if (pipe_i > 0) {
                    pipe_err = 1;
                    ptr_pipe_err = i + 1;
                    if (piped_stop_token > i) piped_stop_token = i;
                } else {
                    err = 1;
                    ptr_err = i + 1;
                    if (stop_token > i) stop_token = i;
                }
            } else if (strcmp(token, "|") == 0 && i != 0) {
                pipe_i = i+1;
                if (stop_token > i) stop_token = i;
            } else if (strcmp(token, "&") == 0) {
                background = TRUE;
                if (stop_token > i) stop_token = i;
            } else if (strcmp(token, "fg") == 0) {
                fg = 1;
            } else if (strcmp(token, "bg") == 0) {
                bg = 1;
            }
            
            argv[i] = token;
            token = strtok_r(NULL, delim, &cmd_cpy);
            // printf("%s \n", argv[i]);
            i++;
        }
        argv[i] = NULL;
        
        if (background == TRUE) {
            int total_length = 0;
            int j = 0;
            while (j < argc && strcmp(argv[j], "&") != 0) {
                total_length += strlen(argv[j]) + 1; 
                j++;
            }
            jname = malloc(sizeof(char) * (total_length + 1)); 
            if (jname == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                return -1;
            }
            jname[0] = '\0';
            for (int i = 0; i < j; i++) {
                strcat(jname, argv[i]);
                if (i < j - 1)
                    strcat(jname, " ");
            }
        } else {
            jname = strdup(*argv);
        }

        /* Because execvp doesnt take in file redirection - only collect command functions */
        char **exec = malloc(sizeof(char *) * (stop_token + 1)); 
        for (int j = 0; j < stop_token; j++) {
            exec[j] = argv[j]; 
        }
        exec[stop_token] = NULL;

        /* Get the second part from cmdline incase of piping */
        char **piped = malloc(sizeof(char *) * (piped_stop_token - pipe_i + 1));
        if (pipe_i > 0) {
            int j = 0;
            for (int i = pipe_i; i < piped_stop_token; i++) {
                piped[j] = argv[i];
                j++;
            }
            piped[j] = NULL;
        }

        if (strcmp("jobs", argv[0]) == 0) {
            printList();
        } else if (fg == TRUE || bg == TRUE) {
            if (fg == TRUE) 
                fg_call();
            else if (bg == TRUE)
                bg_call();
        } else if (pipe_i > 0) {
            /* Pipping */
            if (pipe(pipefd) == -1) {
                perror("# pipe err");
                return -1;
            }            
            pid_t left_pid;
            left_pid = fork();
            /* Frist command, on the left hand side */
            if (left_pid == -1) {
                perror("# Unable to first pipped command");
                return -1;
            } else if (left_pid == 0) {
                setpgid(0, 0);
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);

                if (in) {
                    int fd0 = open(argv[ptr_in],  O_RDONLY | O_CREAT);
                    if (fd0 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd0, STDIN_FILENO);
                    close(fd0);
                } if (out) {
                    int fd1 = open(argv[ptr_out], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd1 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd1, STDOUT_FILENO);
                    close(fd1);
                } if (err) {
                    int fd2 = open(argv[ptr_err], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd2 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd2, STDERR_FILENO);
                    close(fd2);
                }

                if (execvp(exec[0], exec) == -1) {
                    perror("# Unable to execute command");
                    return -1;
                }
            }
            pipedGID = left_pid;

            /* Second command, on the right hand side */
            pid_t right_pid;
            right_pid = fork();
            if (right_pid == -1) {
                perror("# Unable to first second command");
                return -1;
            } else if (right_pid == 0) {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                setpgid(0, left_pid);
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);

                if (pipe_in) {
                    int fd0 = open(argv[ptr_pipe_in], O_RDONLY | O_CREAT);
                    if (fd0 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd0, STDIN_FILENO);
                    close(fd0);
                } if (pipe_out) {
                    int fd1 = open(argv[ptr_pipe_out], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd1 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd1, STDOUT_FILENO);
                    close(fd1);
                } if (pipe_err) {
                    int fd2 = open(argv[ptr_pipe_err], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd2 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd2, STDERR_FILENO);
                    close(fd2);
                }
                if (execvp(piped[0], piped) == -1) {
                    perror("# Unable to execute command");
                    return -1;
                }
            }
            close(pipefd[0]);
            close(pipefd[1]);

            waitpid(left_pid, &status, WUNTRACED);
            waitpid(right_pid, &status, WUNTRACED);

            tcsetpgrp(STDIN_FILENO, getpgid(parentPID));
        } else {
            /* No Pipping */
            pid_t pid;
            pid = fork();
            if (pid == -1) {
                perror("# Unable to execute command");
                return -1;
            } else if (pid == 0) {
                setpgid(0, 0);
                if (in) {
                    int fd0 = open(argv[ptr_in],  O_RDONLY | O_CREAT);
                    if (fd0 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd0, STDIN_FILENO);
                    close(fd0);
                } if (out) {
                    int fd1 = open(argv[ptr_out], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd1 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd1, STDOUT_FILENO);
                    close(fd1);
                } if (err) {
                    int fd2 = open(argv[ptr_err], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd2 == -1) {
                        perror("# Failed to open input file");
                        exit(1);
                    }
                    dup2(fd2, STDERR_FILENO);
                    close(fd2);
                }
                int val = execvp(exec[0], exec);
                if (val == -1) {
                    perror("# Unable to execute command");
                    exit(1);
                }
            } else {
                /* Parent process: shell (yash), waiting for child to finish to avoid zombie state of child */
                setpgid(pid, pid);
                /* If it is not a background process */
                if (!background) {
                    int status;
                    tcsetpgrp(STDIN_FILENO, getpgid(pid));
                    waitpid(pid, &status, WUNTRACED);
                    /* check if the process finished OR stopped (if ^Z update in jobs table else nothing) */
                    if (WIFSIGNALED(status) > 0 || WIFEXITED(status) > 0) {
                        /* change status to DONE in linked list if exited gracefully */
                        if (WTERMSIG(status) != -1 || WEXITSTATUS(status) != -1)
                            updatenode(pid, DONE);
                    } else if (WIFSTOPPED(status) > 0) {
                        if (found(pid) == FALSE) {
                            add(jname, pid, STOPPED, NO_BACKGROUND);
                        } else {
                            updatenode(pid, STOPPED);
                        }
                    }
                    tcsetpgrp(STDIN_FILENO, getpgid(getpid()));
                } else {
                    /* If it is a background process */
                    add(jname, pid, RUNNING, BACKGROUND);
                    tcsetpgrp(STDIN_FILENO, getpgid(parentPID));
                }
            }
        }
        free(argv), free(exec), free(piped), free(jname);
    }
}

void handler(int sig) {
    if (sig == SIGCHLD) {
        struct Job *curr = head;
        while (curr != NULL) {
            int status, val;
            val = waitpid(curr->pID, &status, WNOHANG);
            if (val > 0) {
                if (WTERMSIG(status) == SIGINT) {
                    kill(-curr->pID, SIGINT);
                    removenode(curr->pID);
                } else if (WIFEXITED(status) > 0) {
                    updatenode(curr->pID, DONE);
                }
            /* handles stopped foreground tasks that where originally sent to the background */
            } else if (val == 0) {
                if (WIFSTOPPED(status) > 0) {
                    kill(curr->pID, SIGTSTP);
                    updatenode(curr->pID, STOPPED);
                }
            }
            curr = curr->next;
        }
    } else if (sig == SIGTSTP && pipe_i > 0) {
        kill(-pipedGID, SIGTSTP);
        if (found(pipedGID) == FALSE) {
            add(jname, pipedGID, STOPPED, NO_BACKGROUND);
        } else {
            updatenode(pipedGID, STOPPED);
        }
    } else if (sig == SIGINT && pipe_i > 0) {
        kill(-pipedGID, SIGINT);
    }
   /* SET TERMINAL CONTROL TO YASH */
    tcsetpgrp(STDIN_FILENO, getpgid(parentPID));
}
void printList() {
    struct Job *curr = head;
    int ishead = TRUE;
    int jid_plus;

    /* Removing done jobs first */
    while (curr != NULL) {
        if (strcmp(curr->status, DONE) == 0) {
            printf("[%d]-   %s       %s &\n", curr->jobID, curr->status, curr->name);
            removenode(curr->pID);
        } else if (ishead == TRUE && (curr->background == TRUE) && strcmp(STOPPED, curr->status) != 0) {
            jid_plus = curr->jobID;
            ishead = FALSE;
        }
        curr = curr->next;
    }

    // curr = head;
    // int isFirst = TRUE;
    // while (curr) {
    //     if (isFirst == TRUE) {
    //         if (curr->background == TRUE)
    //             printf("[%d]+   %s       %s &\n", curr->jobID, curr->status, curr->name);
    //         else
    //             printf("[%d]+   %s       %s \n", curr->jobID, curr->status, curr->name);
    //         isFirst = FALSE;
    //     } else {
    //         if (curr->background == TRUE)
    //             printf("[%d]-   %s       %s &\n", curr->jobID, curr->status, curr->name);
    //         else
    //             printf("[%d]-   %s       %s \n", curr->jobID, curr->status, curr->name);
    //     }
    // }

    curr = head;
    struct Job *stack[100];
    int top = -1;

    while (curr != NULL) {
        stack[++top] = curr;
        curr = curr->next;
    }

    while (top >= 0) {
        struct Job *job = stack[top--];
        if (jid_plus == job->jobID) {
            if (job->background == TRUE)
                printf("[%d]+   %s       %s &\n", job->jobID, job->status, job->name);
            else
                printf("[%d]+   %s       %s \n", job->jobID, job->status, job->name);
        } else {
            if (job->background == TRUE)
                printf("[%d]-   %s       %s &\n", job->jobID, job->status, job->name);
            else
                printf("[%d]-   %s       %s \n", job->jobID, job->status, job->name);
        }
    }
}
void add(char *NAME, pid_t PID, char *STAT, int BACK) {
    int max_jid = 1;
    struct Job *p = head;
    while (p != NULL) {
        if (p->jobID >= max_jid)
            max_jid = p->jobID + 1;
        p = p->next;
    }
    /* Creating node */
    Job *curr = (Job *) malloc(sizeof(Job));
    curr->name = strdup(NAME);
    curr->jobID = max_jid;
    curr->pID = PID;
    curr->status = strdup(STAT);
    curr->background = BACK;

    /* Set old head to new node */
   curr->next = head;
   head = curr;
}
void removenode(pid_t PID) {
   struct Job *temp = head, *prev;
   /* If the head is the node to be removed */
   if (temp != NULL && temp->pID == PID) {
      head = temp->next;
      free(temp);
      return;
   }

   /* Find the key to be deleted */
   while (temp != NULL && temp->pID != PID) {
      prev = temp;
      temp = temp->next;
   }

   /* If the JOB is not present */
   if (temp == NULL) return;

   /* Remove the node */
   prev->next = temp->next;
    free(temp);
}
int updatenode(pid_t PID, char *STATUS) {
    struct Job *curr = head;
    while (curr != NULL) {
        if (curr->pID == PID) {
            curr->status = strdup(STATUS);
            return TRUE;
        }
        curr = curr->next;
    }
    return FALSE;
}
void bg_call() {
    struct Job *curr = head;
    int ishead = TRUE;
    while (curr != NULL) {
        if (ishead == TRUE && (curr->background == TRUE) && strcmp(STOPPED, curr->status) != 0)
            ishead = FALSE;
        if (strcmp(curr->status, STOPPED) == 0)
            break;
        curr = curr->next;
    }

    if (curr == NULL) {
        fprintf(stderr, "nothing to run in background\n");
        return;
    }

    curr->background = BACKGROUND;
    if (ishead)
        printf("[%d]+ %s &\n", curr->jobID, curr->name);
    else 
        printf("[%d]- %s &\n", curr->jobID, curr->name);

    updatenode(curr->pID, RUNNING);
    kill(-curr->pID, SIGCONT);
    return;
}
void fg_call() {
    /* Finds the first stopped or running */
    struct Job *curr = head;
    while (curr != NULL) {
        if (curr->background == TRUE || strcmp(curr->status, STOPPED) == 0) {
            break;
        }
        curr = curr->next;
    }
    if (curr == NULL)
        return;
    
    /* update in linked list AND give terminal control to current job */
    /* if it was stopped, change status to running */
    if (strcmp(curr->status, RUNNING) != 0) {
        curr->status = strdup(RUNNING);
        kill(-curr->pID, SIGCONT);
    }
    curr->background = NO_BACKGROUND;
    tcsetpgrp(STDOUT_FILENO, getpgid(curr->pID));
    tcsetpgrp(STDIN_FILENO, getpgid(curr->pID));
    
    /* deep copy copy */
    int c_bg = NO_BACKGROUND;
    char *c_name = strdup(curr->name);
    int c_jid = curr->jobID;
    pid_t c_pid = curr->pID;
    char *c_status = RUNNING;

    /* Removing node */
    removenode(c_pid);

    /* adding */
    add(c_name, c_pid, c_status, c_bg);
    sleep(1);

    printf("%s\n", c_name);
    int status;
    waitpid(c_pid, &status, WUNTRACED);
    return; 
}   
int found(pid_t PID) {
    struct Job *curr = head;
    while(curr != NULL) {
        if (PID == curr->pID)
            return TRUE;
        curr = curr->next;
    }
    return FALSE;
}
void freelist() {
    struct Job *curr = head;
    struct Job *prev = NULL;
    while (curr != NULL) {
        prev = curr;
        curr = curr->next;
        free(prev);
    }
}