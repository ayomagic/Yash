# Yash — Yet Another Shell

## Overview

**Yash** is a simplified Unix-style shell written in C for the EE461S Operating Systems shell project.
The goal of this project is to build a working command-line interpreter that demonstrates core operating system concepts such as:

* process creation with `fork()`
* program execution with `execvp()`
* file descriptor manipulation with `dup2()`
* inter-process communication with `pipe()`
* signal handling
* process groups and terminal control
* basic job control with `jobs`, `fg`, and `bg`

This shell is not intended to be a full replacement for `bash`, but rather an educational implementation of the most important shell mechanisms.

---

## Features Implemented

This shell currently supports:

* Executing external commands using `execvp()`
* Searching the `PATH` environment variable for executables
* Input redirection using `<`
* Output redirection using `>`
* Error redirection using `2>`
* Background execution using `&`
* A single pipe using `|`
* Basic job control:

  * `jobs`
  * `fg`
  * `bg`
* Signal handling for:

  * `SIGCHLD`
  * `SIGINT`
  * `SIGTSTP`
* Foreground terminal control using process groups and `tcsetpgrp()`

---

## Purpose

The purpose of this project is to show how a shell works internally.

When a user types a command, the shell must:

1. read the input line
2. parse the command and detect special symbols
3. create child processes
4. redirect input/output when requested
5. manage foreground and background execution
6. handle stopped and resumed jobs
7. return terminal control back to the shell when appropriate

This project focuses on implementing those operating system concepts directly in C.

---

## How the Code Works

## 1. Main Shell Loop

The shell runs inside an infinite loop in `main()`.

Each iteration:

* prints the prompt `# `
* reads one line of input using `getline()`
* tokenizes the input using `strtok_r()`
* checks for built-in commands like `jobs`, `fg`, and `bg`
* otherwise executes the parsed command as either:

  * a normal command
  * a background command
  * a piped command

This loop acts as the control center for the shell.

---

## 2. Parsing Logic

The command line is parsed in two passes:

### First pass

The code counts how many tokens are present so it can allocate enough memory for `argv`.

### Second pass

The code stores each token into `argv` while also detecting special shell operators such as:

* `<`
* `>`
* `2>`
* `|`
* `&`

During this step, the shell records:

* whether input, output, or error redirection is requested
* whether the command should run in the background
* whether the command contains a pipe
* where the left and right sides of the pipeline begin and end

The result is a parsed command structure represented through arrays and index markers.

---

## 3. Executing Normal Commands

If the command is not built-in and does not contain a pipe, the shell creates a child process using `fork()`.

### In the child process

The child:

* creates a new process group with `setpgid(0, 0)`
* applies redirections using `open()` and `dup2()`
* calls `execvp()` to replace itself with the requested program

### In the parent process

The parent:

* places the child in its own process group
* either waits for it if it is a foreground job
* or records it in the jobs list if it is a background job

Foreground jobs temporarily receive terminal control using `tcsetpgrp()` so that `Ctrl-C` and `Ctrl-Z` affect the job rather than the shell.

---

## 4. File Redirection

The shell supports:

* `<` for input redirection
* `>` for standard output redirection
* `2>` for standard error redirection

Redirection is handled in the child process before calling `execvp()`.

For example:

```bash
cat < input.txt
echo hello > out.txt
wc fakefile.txt 2> err.txt
```

This is implemented by:

* opening the file with `open()`
* replacing the desired file descriptor using `dup2()`
* closing the original file descriptor afterward

---

## 5. Pipe Implementation

The current shell supports **one pipe maximum**.

For a piped command like:

```bash
cat file.txt | wc
```

the shell:

1. creates a pipe with `pipe(pipefd)`
2. forks the left child
3. forks the right child
4. connects:

   * the left child’s `stdout` to the pipe write end
   * the right child’s `stdin` to the pipe read end
5. places both children into the same process group

This design allows the two commands in the pipeline to behave as one job.

---

## 6. Job Control Implementation

Job control is implemented using a singly linked list.

Each job is stored in a `Job` struct:

```c
typedef struct Job {
    char *name;
    int jobID;
    pid_t pID;
    char *status;
    int background;
    struct Job *next;
} Job;
```

Each node stores:

* the command name
* a shell-assigned job number
* the process ID
* the job status (`Running`, `Stopped`, or `Done`)
* whether it is a background job
* a pointer to the next job

### Job helper functions

The shell uses several helper functions to manage jobs:

* `add()` — adds a job to the linked list
* `removenode()` — removes a job by PID
* `updatenode()` — updates a job’s status
* `found()` — checks whether a job already exists
* `printList()` — prints the jobs table
* `fg_call()` — resumes the current job in the foreground
* `bg_call()` — resumes the current job in the background

Jobs are inserted at the head of the list so the most recent job is easy to access.

---

## 7. Built-In Commands

### `jobs`

Prints the current jobs table, including:

* job number
* current marker (`+` or `-`)
* job status
* original command

### `fg`

Brings the most recent stopped or background job to the foreground.

This involves:

* sending `SIGCONT` when needed
* giving terminal control to that job
* waiting for it to finish or stop again

### `bg`

Resumes the most recent stopped job in the background by sending `SIGCONT`.

---

## 8. Signal Handling

The shell installs handlers for:

* `SIGCHLD`
* `SIGINT`
* `SIGTSTP`

### `SIGCHLD`

Used to detect background child state changes and update job statuses.

### `SIGINT`

Used to forward interrupt behavior to the active foreground pipeline when needed.

### `SIGTSTP`

Used to stop foreground pipeline jobs and add or update them in the job table.

Signal handling is a key part of job control because it allows the shell to react when child processes stop, continue, or terminate.

---

## Build Instructions

Compile the shell with:

```bash
gcc -Wall -Wextra -o yash yash.c
```

If your file has a different name, replace `yash.c` with your source file name.

Run it with:

```bash
./yash
```

---

## Example Usage

### Run a normal command

```bash
# ls
```

### Output redirection

```bash
# echo hello > out.txt
```

### Input redirection

```bash
# cat < out.txt
```

### Error redirection

```bash
# wc fakefile.txt 2> err.txt
```

### Background job

```bash
# xeyes &
```

### Stop a job

Run a foreground job, then press:

```text
Ctrl-Z
```

### View jobs

```bash
# jobs
```

### Resume in background

```bash
# bg
```

### Resume in foreground

```bash
# fg
```

### Single pipe

```bash
# cat file.txt | wc
```

---

## Current Limitations

This shell is intentionally limited to the project scope and is **not** a full terminal.

Current limitations include:

* only one pipe is supported
* no support for quoted strings or escaped whitespace
* no append redirection (`>>`)
* no support for shell built-ins such as:

  * `cd`
  * `history`
  * `export`
  * `unset`
* no support for complex shell grammar
* no support for multiple pipelines in a single command
* no advanced input editing or command history

These limitations are expected for a simplified educational shell.

---

## Future Work

A major next step would be to extend the shell into a more complete terminal by supporting **three or more pipes**.

Right now, the pipeline logic is hardcoded for exactly two commands:

* one left command
* one right command
* one `pipefd[2]`

That works for a single `|`, but not for commands like:

```bash
cat file.txt | grep hello | wc
```

### Planned improvement for multi-pipe support

To support an arbitrary number of pipes, the shell would need to be redesigned so that:

1. the parser splits the command line into **N command stages**
2. the shell creates **N - 1 pipes**
3. the shell forks **N children**
4. each child connects to the correct input/output pipe ends
5. all children in the pipeline are placed into the same process group
6. the parent closes all unused pipe file descriptors
7. the full pipeline is managed as one job

This would allow commands such as:

```bash
cat file.txt | grep hello | sort | uniq | wc
```

to run correctly.

### Other future improvements

Additional future work could include:

* support for `>>` append redirection
* support for quoted arguments
* support for escaped spaces
* support for shell built-ins such as `cd`
* support for command history
* improved signal-safe job table updates
* more robust memory management
* cleaner parser abstraction
* improved error handling
* automated tests for shell behavior

These additions would make the shell behave more like a real Unix terminal while still preserving the educational value of the project.

---

## Educational Takeaways

This project demonstrates several important operating systems concepts in practice:

* how shells launch processes
* how Unix file descriptors are redirected
* how inter-process communication works with pipes
* how signals affect parent and child processes
* how process groups allow terminal job control
* how a shell keeps track of running and stopped jobs

Building this shell helped reinforce the connection between C programming and operating system behavior.

---

## Summary

Yash is a simplified shell that implements the core mechanisms behind command execution, redirection, pipelines, and job control in Unix-like systems. Although it currently supports only a subset of shell features, it provides a strong foundation for understanding how terminals work internally and can be extended further into a more complete shell with support for multi-stage pipelines and richer parsing.
