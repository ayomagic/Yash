Yash — Yet Another Shell
Overview
yash is a small Unix-style shell written in C for the EE461S Operating Systems shell project. Its purpose is to demonstrate the core mechanics behind a command-line interpreter: reading user input, parsing commands, creating child processes, handling file redirection, supporting a single pipeline, and implementing basic job control through jobs, fg, and bg. The assignment scope requires support for redirection, one pipe at most, signal handling, background jobs, and a #  prompt, and this implementation is built around that feature set.  
Rather than trying to behave like a full production shell such as bash, this project focuses on the OS concepts underneath a shell: fork, execvp, waitpid, dup2, process groups, terminal control, and signal-driven job management. The code is therefore best understood as an educational shell that implements the major features required by the project while leaving room for future expansion into a more complete terminal.  
Supported Functionality
The current implementation supports execution of external commands through execvp, which means commands are searched through the system PATH and executed as child processes. It also supports input, output, and error redirection using <, >, and 2>, background execution with &, a single pipeline using |, and the built-in job-control commands jobs, fg, and bg. The shell installs handlers for SIGCHLD, SIGINT, and SIGTSTP, and uses process groups plus tcsetpgrp to manage foreground execution and job suspension/resumption.  
The prompt is printed as #  before each command is read, matching the assignment requirement. The code uses getline to read each line, tokenizes input with strtok_r, and tracks special tokens such as <, >, 2>, |, and & during parsing.  
High-Level Design
The shell runs inside an infinite loop in main. At the start of each iteration, it resets parsing flags, prints the prompt, reads one line of input, tokenizes the command, and decides whether the command is a built-in operation (jobs, fg, or bg) or an external command that should be executed in a child process. This loop is the central control flow of the shell. 
The parser performs two passes over the input. The first pass counts tokens so memory for argv can be allocated. The second pass records all tokens while also identifying the positions of redirection operators, the optional background marker, and the boundary between the left and right sides of a pipeline. From that parsed state, the code builds two execution arrays: one for the main command and, when needed, one for the second stage of the pipeline. 
Command Execution
For a normal command with no pipe, the shell calls fork(). In the child process, it creates a new process group with setpgid, applies any requested redirections using open, dup2, and close, and finally invokes execvp with the prepared argument vector. In the parent, the shell either waits for the child if it is a foreground job or immediately records it in the jobs list if it is a background job. 
Foreground jobs temporarily receive terminal control through tcsetpgrp, allowing signals such as Ctrl-C and Ctrl-Z to affect the child job rather than killing or stopping the shell itself. Once the foreground job exits or stops, terminal control is returned to the shell. This mirrors the core job-control behavior expected in the project spec.  
Redirection
File redirection is handled in the child process just before execvp. The parser records the token index immediately after <, >, or 2>, and the child later uses that filename to open the requested file descriptor. Standard input is replaced with the input file for <, standard output is redirected for >, and standard error is redirected for 2>. This is done with dup2, which makes the command behave as though it were reading from or writing directly to those files.  
Single-Pipe Support
The current code supports one pipeline by creating exactly one pipefd[2] pair and forking two child processes: a left-side process and a right-side process. The left child redirects its standard output to the write end of the pipe, and the right child redirects its standard input to the read end. Both children are placed into the same process group so that they can be managed together as one pipeline job. This matches the project requirement that only one pipe must be supported.  
Because the design stores only one pipe position (pipe_i), one pipe descriptor array (pipefd), and one secondary command array (piped), it is intentionally limited to two commands total. That design keeps the implementation manageable for the lab but is also the main reason the shell cannot yet support pipelines of three or more commands. 
Job Control Design
Background and stopped jobs are stored in a singly linked list of Job nodes. Each node stores the original command name, a shell-assigned job number, the process ID, the current status string (Running, Stopped, or Done), whether the job is backgrounded, and a pointer to the next job. The helper functions add, removenode, updatenode, found, and printList provide the basic job table operations. 
New jobs are inserted at the head of the list, which makes the most recent job easy to find. The jobs command walks the list, prints completed jobs as Done, removes them from the active list, and then prints the remaining jobs in display order using a temporary stack. The bg command finds a stopped job, marks it as a background job, sends SIGCONT, and returns immediately. The fg command brings the most recent runnable job to the foreground, restores terminal control to that job, and waits for it to complete or stop again.  
Signals and Process Groups
The shell installs handlers for SIGCHLD, SIGINT, and SIGTSTP. SIGCHLD is used to detect child state changes so that completed jobs can be marked as Done. SIGINT and SIGTSTP are also handled so that pipeline process groups can be signaled appropriately when a foreground pipeline is interrupted or stopped. Process groups are critical here: by grouping related processes together, the shell can send signals to an entire job instead of just one PID.  
Purpose of This Implementation
This code is meant to show how a shell bridges user input and the operating system. A user types a command, the shell interprets it, spawns child processes, configures file descriptors, manages job states, and arbitrates access to the terminal. In other words, the project is not just about “running commands,” but about exposing the process-management and terminal-control mechanisms that normal shells hide from users.  
Current Limitations
This version is intentionally centered on the lab requirements rather than full shell compatibility. Parsing assumes whitespace-separated tokens, only one pipe is supported, and the built-ins are limited to jobs, fg, and bg, which is consistent with the project restrictions. The code also stores job metadata in a way that is sufficient for the assignment but could be improved for richer command history and more exact terminal behavior.  
In addition, some implementation areas would need to be hardened for a production-quality shell. Examples include stricter parser validation, more robust memory management, safer signal handling patterns, and fuller support for shell syntax such as quoting, append redirection, and multi-stage pipelines. These are natural next steps rather than failures of the current educational design. 
Future Work
The biggest next step is to generalize the pipeline implementation from a fixed two-command model into an arbitrary-length pipeline model. Instead of storing a single pipe boundary and a single piped command array, the parser should build a dynamic array of command objects, where each command stores its own argument list and any redirections associated with that stage. Once the input is represented that way, the shell can create N - 1 pipes for N commands and fork one child per stage in a loop. Each stage would connect its standard input to the previous pipe’s read end and its standard output to the next pipe’s write end, except for the first and last stages, which would keep normal input or output unless redirected. All children in the pipeline should be placed into the same process group so the entire chain can be stopped, resumed, or killed together.
A second major improvement would be a more complete parser. A fuller shell should support quoted strings, escaped whitespace, append redirection with >>, multiple pipes, and eventually shell built-ins such as cd, pwd, exit, history, export, and unset. The current parser is intentionally simple and based on whitespace tokenization, which is fine for this project but not sufficient for a real terminal experience.  
Another important area is signal safety and job-table synchronization. A more mature design would minimize the amount of work done directly inside signal handlers and would instead use safer synchronization patterns so that job updates happen in a controlled place in the main loop. This would make the shell more reliable under rapid child exits, repeated background jobs, and frequent stop/resume events. 
Finally, the user experience could be improved by integrating readline or an equivalent line editor for history and editing support, improving error messages, and building a formal test suite that covers redirection, job control, pipeline behavior, invalid commands, and terminal signal scenarios. Together, these changes would move yash from a successful course shell into a more realistic terminal environment.  
Example Direction for Multi-Pipe Support
A clean design for supporting three or more pipes would look like this:


Parse the command line into a list of command stages separated by |.


For N stages, allocate N - 1 pipes.


Fork N children in a loop.


In child i, connect input from pipe i - 1 if i > 0, and connect output to pipe i if i < N - 1.


Apply per-stage redirections before calling execvp.


Put every stage into the same process group so the pipeline behaves like one foreground or background job.


In the parent, close all pipe file descriptors and wait on the pipeline as one job.


If you want, I can turn this into a cleaner “submission-ready” README with a more polished tone and a short usage section.
