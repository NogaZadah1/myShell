myShell
Command Execution and Monitoring Shell
Authored Noga Zadah
==Description==
This project implements a custom shell-like command-line tool capable of executing user commands with enhanced monitoring and control. It offers functionality similar to Unix shells, enriched with execution time tracking, resource limitation, command validation, and parallelized computation capabilities.
•	The shell supports features such as:
•	Single and piped command execution
•	Detection and blocking of dangerous commands loaded from a file
•	Setting resource limits (CPU, memory, file size, open files)
•	Real-time display of statistics: command count, blocked commands, execution times
•	Background command execution
•	Redirection of standard error output
•	Custom implementation of the "tee" command
•	New: Matrix calculation utility (mcalc) supporting ADD and SUB in parallel
Program DATABASE:
The program uses several internal structures:
1.	A list of dangerous commands (`DangerCommand`) to block or warn against specific input.
2.	Resource limitation structures (`LimitSpeac`) representing soft and hard limits for system resources.
3.	Matrix handling structures (`Matrix`, `McalcParsed`) used in the new `mcalc` functionality.
4.	A simulated environment that logs command execution times and limits using system signals and process control.
Functions:
Main functionality is divided across several core methods:
5.	`run_command` – executes individual commands and updates runtime statistics.
6.	`run_pipe_command` – executes two commands connected by a pipe.
7.	`load_dangerous_commands` – loads dangerous commands from a user-provided file.
8.	`check_if_blocked_command` – checks if a command matches or is similar to a dangerous command.
9.	`my_tee` – custom function for writing input to multiple files.
10.	`handle_rlimit_*` functions – manage and apply runtime limits on resources.
11.	`parse_mcalc_struct_based`, `matrix_add`, `matrix_sub`, `run_parallel_matrix_calc` – handle matrix parsing and computation using threads.
12.	Signal handlers – manage system-level events like CPU limit exceeded or memory access errors.
==Program Files==
•	ex2.c – main program file, contains all logic and entry point
•	dangerous_comand.txt – file listing dangerous commands (input)
•	exec_times.log – log file for execution times (output)
•	Makefile – not included but can be created manually for automation
==How to compile?==
To compile:
gcc ex2.c -o ex2 -Wall -pthread
To run:
./ex2 <dangerous_command_file> <log_file>
==Input==
•	User input via standard input (commands)
•	File input containing dangerous commands
==Output==
•	Console output showing command execution and statistics
•	Log file recording execution times
•	Optional stderr redirection to files using standard shell syntax
•	Matrix computation results from `mcalc` printed to console
