//
// Created by student on 5/22/25.
//
#include <string.h>
#include <sys/resource.h>

#define MAXSIZE 1025
#define MAXARG 7
#define MAX_LIMITS 4
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>


typedef struct {
    char *dang_cmd;//the dangerus cmd
} DangerCommand;

typedef struct {
    int resurce;
    rlim_t soft;
    rlim_t hard;
}LimitSpeac;

typedef struct {
    int rows;
    int cols;
    double *data;
}Matrix;

typedef struct {
    char **matrices;
    int matrix_count;
    char *operation;
    int valid;
}McalcParsed;


int check_args(char *args);
void splitWords(char ***words, char* args, int count);
void terminal (char* danger_name, char *log_name) ;
double calc_time (struct timespec start, struct timespec end);
void min_max_sec (double *min, double *max, double curr);
int load_dangerous_commands(char *filename, DangerCommand **danger_list);
int check_if_blocked_command(char **args, int count_arg, DangerCommand *danger_list, int danger_count, int *blocked_count);
void free_dangerous_commands(DangerCommand *danger_list, int danger_count);
int count_lines(char *name);
int checkPipe (char * str);
int run_command(char **words, int count, FILE *fp,DangerCommand *danger_list, int danger_count, int *counter_success, int *dangerous_comand,
                double *last_cmd, double *avg, double *min, double *max, int isBackground);
void run_pipe_command(char *left_cmd, char *right_cmd,
                      FILE *fp,
                      DangerCommand *danger_list, int danger_count,
                      int *counter_success, int *dangerous_comand,
                      double *last_cmd, double *avg, double *min, double *max);
void trim_whitespace(char *str);
void my_tee(char **args, int arg_count);
int handle_rlimit_set_command(char **words, int count, LimitSpeac *limits);
rlim_t parse_size(const char *str);
void handle_rlimit_show_command();
void apply_rlimits(LimitSpeac limits[]);
void handler_sigchiled(int signum);
void check_and_extract_stderr_redirect(char **args, int *count, char **filename, int *redirect_flag);
int check_args_rlimit(char *str);
void handle_sigcpu(int signo);
void handle_sigfsz(int signo);
void handle_sigmem(int signo);
void handle_signof(int signo);
int check_args_mcalc(char *str);
McalcParsed parse_mcalc_struct_based(char **args, int count);
Matrix parse_matrix(const char *str);
Matrix matrix_sub(const Matrix *a, const Matrix *b);
Matrix matrix_add(const Matrix *a, const Matrix *b);
Matrix run_parallel_matrix_calc(Matrix *matrices, int count, const char *operation);
void *thread_matrix_op(void *arg);
void free_matrix_array(Matrix *matrices, int count, bool free_data);
void free_mcalc_parsed(McalcParsed *parsed);



int main(int argc, char *argv[]) {
    signal(SIGXCPU, handle_sigcpu);   // cpu
    signal(SIGXFSZ, handle_sigfsz);   // files
    signal(SIGSEGV, handle_sigmem);   // memory
    signal(SIGUSR1, handle_signof);
    terminal(argv[1], argv[2]);
    return 0;
}

void terminal(char *danger_name, char *log_name) {
    FILE *fp = fopen(log_name, "a");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }

    char **words;
    pid_t pid;
    double min = 0, max = 0, avg = 0, last_cmd = 0;
    int counter_success = 0, dangerous_comand = 0;
    char str[MAXSIZE];
    DangerCommand *danger_list;
    int danger_count = 0;
    danger_count = load_dangerous_commands(danger_name, &danger_list);

    printf("#cmd:%d|#dangerous_cmd_blocked:%d|last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time:%.5f>>",
           counter_success, dangerous_comand, last_cmd, avg, min, max);

    fgets(str, MAXSIZE, stdin);
    str[strcspn(str, "\n")] = '\0';

    while (strcmp(str, "done") != 0) {
        int isBackground = 0;
        int pipe_index = checkPipe(str);

        if (pipe_index != -1) {
            // Pipe command
            char temp[MAXSIZE];
            strcpy(temp, str);
            char *left_cmd = strtok(temp, "|");
            char *right_cmd = strtok(NULL, "|");
            trim_whitespace(left_cmd);
            trim_whitespace(right_cmd);
            run_pipe_command(left_cmd, right_cmd, fp, danger_list, danger_count,
                             &counter_success, &dangerous_comand,
                             &last_cmd, &avg, &min, &max);
        } else {
            // Handle regular or special command
            int count;
            if (strncmp(str, "rlimit set", 10) == 0) {
                count = check_args_rlimit(str);
            } else {
                count = check_args(str);
            }

            if (count > 0) {

                splitWords(&words, str, count);
                
                trim_whitespace(words[0]);  


                // Background check
                if (strcmp(words[count - 1], "&") == 0) {
                    isBackground = 1;
                    words[count - 1] = NULL;
                    count--;
                }

                run_command(words, count, fp, danger_list, danger_count,
                            &counter_success, &dangerous_comand,
                            &last_cmd, &avg, &min, &max, isBackground);

                for (int i = 0; i < count; i++) free(words[i]);
                free(words);
                goto NEXT_PROMPT;
            }
        }

    NEXT_PROMPT:
        fflush(stdout);
        printf("#cmd:%d|#dangerous_cmd_blocked:%d|last_cmd_time:%.5f|"
               "avg_time:%.5f|min_time:%.5f|max_time:%.5f>>",
               counter_success, dangerous_comand, last_cmd, avg, min, max);

        if (!fgets(str, MAXSIZE, stdin)) break;
        str[strcspn(str, "\n")] = '\0';
    }

    printf("%d\n", dangerous_comand);
    free_dangerous_commands(danger_list, danger_count);
    fclose(fp);
}

int check_args(char *str) {
    int count = 0;
    int in_word = 0;
    int check = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ' ' && str[i + 1] == ' ') {
            printf("ERR_SPACE\n");
            check = 1;
        }
        if (str[i] != ' ' && in_word == 0) {
            in_word = 1;
            count++;
        } else if (str[i] == ' ') {
            in_word = 0;
        }
    }
    if (count > MAXARG) {
        printf("ERR_ARGS\n");
        return -1;
    }
    if (check == 1) {
        return -1;
    }
    return count;
}

void splitWords(char ***words, char* args, int count){
    *words = (char**)malloc((count + 1) * sizeof(char*));
    if (*words == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    char *args_copy = strdup(args);
    if (args_copy == NULL) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    int word_index = 0;

    char* token = strtok(args_copy, " ");
    while (token != NULL && word_index < count) {
        (*words)[word_index] = (char*)malloc((strlen(token) + 1) * sizeof(char));
        if ((*words)[word_index] == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        strcpy((*words)[word_index], token);
        word_index++;
        token = strtok(NULL, " ");
    }

    (*words)[word_index] = NULL; // NULL-terminate the array
    free (args_copy);
}

double calc_time (struct timespec start, struct timespec end) {
    double sec = end.tv_sec - start.tv_sec;
    double nsec= end.tv_nsec - start.tv_nsec;
    if (nsec < 0) {
        sec -= 1;
        nsec += 1e9;
    }
    return sec + nsec / 1e9;
}

void min_max_sec (double *min, double *max, double curr) {
    if (*min == 0 && *max == 0) {
        *min = curr;
        *max = curr;
    }
    if (* min > curr) {
        *min = curr;
    }
    if (* max < curr) {
        *max = curr;
    }
}

int load_dangerous_commands(char *filename, DangerCommand **danger_list) {
    if (filename == NULL) {
        fprintf(stderr, "Error: filename is NULL\n");
        return -1;
    }
    int max_size = count_lines(filename);
    if (max_size <= 0) {
        fprintf(stderr, "No dangerous commands loaded.\n");
        return 0;
    }
    *danger_list = (DangerCommand*)malloc(max_size * sizeof(DangerCommand));
    FILE *dangerous_commands = fopen(filename, "r");
    if (!dangerous_commands) {
        perror("fopen");
        return -1;
    }
    char line[MAXSIZE];
    int danger_count = 0;
    while (fgets(line, sizeof(line), dangerous_commands)) {
        line[strcspn(line, "\n")] = '\0';
        (*danger_list)[danger_count].dang_cmd = strdup(line);
        danger_count++;
    }
    fclose(dangerous_commands);
    return danger_count;
}

int check_if_blocked_command(char **args, int count_arg, DangerCommand *danger_list, int danger_count, int *blocked_count) {
    char full_cmd[MAXSIZE] = "";
    for (int i = 0; i < count_arg; i++) {
        strcat(full_cmd, args[i]);
        if (i != count_arg - 1) strcat(full_cmd, " ");
    }

    for (int i = 0; i < danger_count; i++) {
        if (strcmp(full_cmd, danger_list[i].dang_cmd) == 0) {
            printf("ERR: Dangerous command detected (\"%s\"). Execution prevented.\n", full_cmd);
            (*blocked_count)++;
            return 1;
        } else {
            char temp[MAXSIZE];
            strcpy(temp, danger_list[i].dang_cmd);
            char *base_cmd = strtok(temp, " ");
            if (base_cmd && strcmp(args[0], base_cmd) == 0) {
                printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n", full_cmd);
                break;
            }
        }
    }
    return 0;
}

void free_dangerous_commands(DangerCommand *danger_list, int danger_count) {
    for (int i = 0; i < danger_count; i++) {
        free(danger_list[i].dang_cmd);
        danger_list[i].dang_cmd = NULL;
    }
    free(danger_list);
}

int count_lines(char *name) {
    FILE *fp = fopen(name, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    int count = 0;
    char line[MAXSIZE];
    while (fgets(line, sizeof(line), fp)) {
        int only_whitespace = 1;
        for (int i = 0; line[i] != '\0'; i++) {
            if (!isspace((unsigned char)line[i])) {
                only_whitespace = 0;
                break;
            }
        }
        if (!only_whitespace) count++;
    }
    fclose(fp);
    return count;
}

int checkPipe (char * str) {
 for (int i = 0; str[i] != '\0'; i++) {
   if (str[i] == '|') {
       return i;
   }
 }
    return -1;
}

int run_command(char **words, int count, FILE *fp,DangerCommand *danger_list, int danger_count, int *counter_success, int *dangerous_comand,
                double *last_cmd, double *avg, double *min, double *max , int isBackground ){

    signal(SIGCHLD, handler_sigchiled);
    int is_blocked = check_if_blocked_command(words, count, danger_list, danger_count, dangerous_comand);
    if (is_blocked) {
        return 0;
    }
    char *stderr_fillname = NULL;
    int  redirect_stderr =0;
    check_and_extract_stderr_redirect(words, &count, &stderr_fillname, &redirect_stderr);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    LimitSpeac limit_speac[MAX_LIMITS] = {0};
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0)
    {
        if (strcmp(words[0], "rlimit") == 0 && count >= 2 && strcmp(words[1], "show") == 0) {
            handle_rlimit_show_command();
            exit(0);
        }
        if (strcmp(words[0], "rlimit") == 0 && strcmp(words[1], "set") == 0) {
            int start_cmd = handle_rlimit_set_command(words, count, limit_speac);
            apply_rlimits(limit_speac);
            if (start_cmd>= count) {
                exit(0);
            }
            if (redirect_stderr) {
                FILE *stderr_file = fopen(stderr_fillname,  "a");
                if (stderr_file == NULL) {
                    perror("fopen");
                    exit(1);
                }
                dup2(fileno(stderr_file), STDERR_FILENO);
                fclose(stderr_file);
            }

            execvp(words[start_cmd], &words[start_cmd]);
            perror("execvp");
            exit(255);
        }

        if (strcmp(words[0], "mcalc") == 0) {
            int mat_argc = count - 1;
            char **mat_words = &words[1]; 

            McalcParsed parsed = parse_mcalc_struct_based(mat_words, mat_argc);

            if (!parsed.valid) {
                printf("ERR_MAT_INPUT\n");
                free_mcalc_parsed(&parsed);
                exit(1);
            } else {
                Matrix *matrices = malloc(sizeof(Matrix) * parsed.matrix_count);
                if (!matrices) {
                    perror("malloc");
                    free_mcalc_parsed(&parsed);
                    exit(1);
                }

                for (int i = 0; i < parsed.matrix_count; i++) {
                    matrices[i] = parse_matrix(parsed.matrices[i]);
                    if (!matrices[i].data) {
                        free_matrix_array(matrices, i, true);
                        free_mcalc_parsed(&parsed);
                        exit(1);
                    }
                }

                Matrix result = run_parallel_matrix_calc(matrices, parsed.matrix_count, parsed.operation);

                printf("# Output: (%d,%d:", result.rows, result.cols);
                for (int i = 0; i < result.rows * result.cols; i++) {
                    printf("%.0f", result.data[i]);
                    if (i < result.rows * result.cols - 1) {
                        printf(",");
                    }
                }
                printf(")\n");
                free(result.data);

                free_matrix_array(matrices, parsed.matrix_count, false);
                free_mcalc_parsed(&parsed);
            }
            exit(0);
        }

        if (redirect_stderr) {
            FILE *stderr_file = fopen(stderr_fillname,  "a");
            if (stderr_file == NULL) {
                perror("fopen");
                exit(1);
            }
            dup2(fileno(stderr_file), STDERR_FILENO);
            fclose(stderr_file);
        }

        execvp(words[0], words);
        perror("execvp");
        exit(255);
    }
    int status;
    if (!isBackground) {

        bool fsize = false;
        for(int i = 0; i < MAX_LIMITS; i++) {
            if(limit_speac[i].resurce == RLIMIT_FSIZE){
                fsize = true;
            }
        }

        wait(&status);
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGXCPU) printf("CPU time limit exceeded!\n");
            else if (sig == SIGXFSZ) printf("File size limit exceeded!\n");
            else printf("%s\n", strsignal(sig));
        } else if (WIFEXITED(status)) {
            int ec = WEXITSTATUS(status);
            if (ec == 0 || ec == 255) {
                (*counter_success)++;
                *last_cmd = calc_time(start, end);
                min_max_sec(min, max, *last_cmd);
                *avg = (((*avg) * ((*counter_success) - 1)) + *last_cmd) / (*counter_success);
                fprintf(fp, "%s : %.5f sec\n", words[0], *last_cmd);
                fflush(fp);
            }else if (fsize) {
                printf("File size limit exceeded!\n");
            }
            else {
                printf("Process exited with error code %d\n", ec);
           }
        }
    }
    return 1;
}


void run_pipe_command(char *left_cmd, char *right_cmd,
                      FILE *fp,
                      DangerCommand *danger_list, int danger_count,
                      int *counter_success, int *dangerous_comand,
                      double *last_cmd, double *avg, double *min, double *max) {
    struct timespec start, end;
    struct timespec start2, end2;

     trim_whitespace(left_cmd);
     trim_whitespace(right_cmd);


    LimitSpeac limits_left[MAX_LIMITS] = {0};
    LimitSpeac limits_right[MAX_LIMITS] = {0};
    int start_index_left = 0, start_index_right = 0;

    int count_left = check_args(left_cmd);
    int count_right = check_args(right_cmd);
    //LimitSpeac* limit_speac;

    if (count_left <= 0 || count_right <= 0) {
        fprintf(stderr, "[ERROR] Invalid arguments in pipe command.\n");
    }else {
        char **left_args, **right_args;
        splitWords(&left_args, left_cmd, count_left);
        splitWords(&right_args, right_cmd, count_right);

        char *left_stderr_filename = NULL;
        char *right_stderr_filename = NULL;
        int left_redirect_stderr = 0;
        int right_redirect_stderr = 0;

        check_and_extract_stderr_redirect(left_args, &count_left, &left_stderr_filename, &left_redirect_stderr);
        check_and_extract_stderr_redirect(right_args, &count_right, &right_stderr_filename, &right_redirect_stderr);


        if (check_if_blocked_command(left_args, count_left, danger_list, danger_count, dangerous_comand) ||
            check_if_blocked_command(right_args, count_right, danger_list, danger_count, dangerous_comand)) {
            for (int i = 0; i < count_left; i++) free(left_args[i]);
            for (int i = 0; i < count_right; i++) free(right_args[i]);
            free(left_args);
            free(right_args);
            return;
            }

        int fd[2];
        if (pipe(fd) == -1) {
            perror("pipe");
            if (left_args) {// if fails so free memory
                for (int i = 0; i < count_left; i++) free(left_args[i]);
                free(left_args);
            }
            if (right_args) {
                for (int i = 0; i < count_right; i++) free(right_args[i]);
                free(right_args);
            }
            return;
        }
        clock_gettime(CLOCK_MONOTONIC, &start);  // start time for left command
        pid_t pid1 = fork();
        if (pid1 == -1) {
            perror("fork (left)");
            if (left_args) {// if fails so free memory
                for (int i = 0; i < count_left; i++) free(left_args[i]);
                free(left_args);
            }
            if (right_args) {
                for (int i = 0; i < count_right; i++) free(right_args[i]);
                free(right_args);
            }
            return;
        }

        if (pid1 == 0) {
            if (left_redirect_stderr) {
                FILE *stderr_file = fopen(left_stderr_filename, "a");
                if (stderr_file == NULL) {
                    perror("fopen");
                    exit(1);
                }
                dup2(fileno(stderr_file), STDERR_FILENO);
                fclose(stderr_file);
            }
            dup2(fd[1], STDOUT_FILENO);
            close(fd[0]);
            close(fd[1]);
            if (strcmp(left_args[0], "rlimit") == 0 && strcmp(left_args[1], "show") == 0) {
                handle_rlimit_show_command();
                exit(0);
            }
            if (strcmp(left_args[0], "rlimit") == 0 && strcmp(left_args[1], "set") == 0) {
                handle_rlimit_set_command(left_args, count_left, limits_left);
                handle_rlimit_show_command();
                exit(0);
            }
            apply_rlimits(limits_left);
            execvp(left_args[0], left_args);
            perror("[ERROR] execvp (left)");
            exit(EXIT_FAILURE);
        }
        clock_gettime(CLOCK_MONOTONIC, &start2); // start time for right command
        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("fork (right)");
            int status_pid1_temp;
            waitpid(pid1, &status_pid1_temp, 0);
            if (left_args) {
                for (int i = 0; i < count_left; i++) free(left_args[i]);
                free(left_args);
            }
            if (right_args) {
                for (int i = 0; i < count_right; i++) free(right_args[i]);
                free(right_args);
            }
            return;
        }

        if (pid2 == 0) {
            if (right_redirect_stderr) {
                FILE *stderr_file = fopen(right_stderr_filename, "a");
                if (stderr_file == NULL) {
                    perror("fopen");
                    exit(1);
                }
                dup2(fileno(stderr_file), STDERR_FILENO);
                fclose(stderr_file);
            }
            dup2(fd[0], STDIN_FILENO);
            close(fd[1]);
            close(fd[0]);
            if (strcmp(right_args[0], "my_tee") == 0) {
                my_tee(right_args, check_args(right_cmd));
                exit(0);
            }
            if (strcmp(right_args[0], "rlimit") == 0 && strcmp(right_args[1], "show") == 0) {
                handle_rlimit_show_command();
                exit(0);
            }
            if (strcmp(right_args[0], "rlimit") == 0 && strcmp(right_args[1], "set") == 0) {
                handle_rlimit_set_command(right_args, count_right, limits_right);
            }
            apply_rlimits(limits_right);
            execvp(right_args[0], right_args);
            perror("[ERROR] execvp (right)");
            exit(EXIT_FAILURE);
        }

        close(fd[0]);
        close(fd[1]);
        int status1, status2;
        bool left_cmd_successful = false;
        bool right_cmd_successful = false;
        double left_cmd_time = 0.0, right_cmd_time = 0.0;

        // Wait for the first child (left command)
        waitpid(pid1, &status1, 0);
        clock_gettime(CLOCK_MONOTONIC, &end); // end time for left command
        left_cmd_time = calc_time(start, end);

        if (WIFEXITED(status1)) {
            int exit_code = WEXITSTATUS(status1);
            if (exit_code == 0) {
                left_cmd_successful = true;
            } else {
                fprintf(stderr, "[ERROR] Left command (%s) exited with status %d\n", left_args[0] ? left_args[0] : "Unknown", exit_code);
            }
        } else if (WIFSIGNALED(status1)) {
            int sig = WTERMSIG(status1);
            fprintf(stderr, "[TERMINATED] Left command (%s) by signal: %s\n", left_args[0] ? left_args[0] : "Unknown", strsignal(sig));
        }

        // Wait for the second child (right command)
        waitpid(pid2, &status2, 0);
        clock_gettime(CLOCK_MONOTONIC, &end2); // end time for right command
        right_cmd_time = calc_time(start2, end2);

        if (WIFEXITED(status2)) {
            int exit_code = WEXITSTATUS(status2);
            if (exit_code == 0) {
                right_cmd_successful = true;
            } else {
                fprintf(stderr, "[ERROR] Right command (%s) exited with status %d\n", right_args[0] ? right_args[0] : "Unknown", exit_code);
            }
        } else if (WIFSIGNALED(status2)) {
            int sig = WTERMSIG(status2);
            fprintf(stderr, "[TERMINATED] Right command (%s) by signal: %s\n", right_args[0] ? right_args[0] : "Unknown", strsignal(sig));
        }


        // Update the average, min, max times
        *last_cmd = (left_cmd_time + right_cmd_time) / 2.0;  // average time for both commands
        min_max_sec(min, max, *last_cmd);
        *avg = (((*avg) * (*counter_success) + *last_cmd) / (*counter_success + 1));
        (*counter_success)++;

        fprintf(fp, "Left Command Time: %.5f sec\n", left_cmd_time);
        fprintf(fp, "Right Command Time: %.5f sec\n", right_cmd_time);
        fprintf(fp, "Total Time: %.5f sec\n", *last_cmd);
        fflush(fp);

        // Free memory
        for (int i = 0; i < count_left; i++) free(left_args[i]);
        for (int i = 0; i < count_right; i++) free(right_args[i]);
        free(left_args);
        free(right_args);
    }
}

void trim_whitespace(char *str) {
    //trim space b4
    char *start = str, *end;
    while (isspace(*start)) start++;
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';  //
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

void my_tee(char **args, int arg_count) {
    int append_mode = 0;
    int file_start_index = 1;
    if (arg_count >= 2 && strcmp(args[1], "-a") == 0) {
        append_mode = 1;
        file_start_index = 2;
    }
    if (arg_count <= file_start_index) {
        fprintf(stderr, "[ERROR] tee: no output files specified\n");
        return;
    }
    int file_count = arg_count - file_start_index;
    FILE **fps = malloc(file_count * sizeof(FILE *));
    if (!fps) {
        perror("malloc");
        return;
    }
    for (int i = 0; i < file_count; ++i) {
        fps[i] = fopen(args[file_start_index + i], append_mode ? "a" : "w");
        if (!fps[i]) {
            perror(args[file_start_index + i]);
            for (int j = 0; j < i; ++j) fclose(fps[j]);
            free(fps);
            return;
        }
    }
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        fputs(buffer, stdout);
        for (int i = 0; i < file_count; ++i) {
            fputs(buffer, fps[i]);
        }
    }
    for (int i = 0; i < file_count; ++i) fclose(fps[i]);
    free(fps);
}

int handle_rlimit_set_command(char **words, int count, LimitSpeac *limits) {
    
    int limit_index = 2;
    int i;//cmd start
    for (i = 2; i < count; i++) {
        if (strchr(words[i], '=') ==0 ) {
            i++; //
            break;
        }
        char *res_str = strtok(words[i], "=");
        char *val_str = strtok(NULL, "=");
        if (!res_str || !val_str) {
            fprintf(stderr, "Invalid rlimit syntax: expected resource=value\n");
            exit(1);
        }

        char *colon = strchr(val_str, ':');
        char *soft_str = val_str;
        char *hard_str = NULL;

        if (colon) {
            *colon = '\0';
            hard_str = colon + 1;
        }
        int resource;
        struct rlimit rl;// hard and soft limit
        rlim_t soft, hard;
        if (hard_str == NULL) {
            hard_str = soft_str;
        }

        if (strcmp(res_str, "cpu") == 0) {
            limits[limit_index].resurce = RLIMIT_CPU;
            soft = strcmp(soft_str, "unlimited") == 0 ? RLIM_INFINITY : (rlim_t)atoi(soft_str);
            hard = strcmp(hard_str, "unlimited") == 0 ? RLIM_INFINITY : (rlim_t)atoi(hard_str);
        } else if (strcmp(res_str, "mem") == 0) {
            limits[limit_index].resurce = RLIMIT_AS;
            soft = strcmp(soft_str, "unlimited") == 0 ? RLIM_INFINITY : parse_size(soft_str);
            hard = strcmp(hard_str, "unlimited") == 0 ? RLIM_INFINITY : parse_size(hard_str);
        } else if (strcmp(res_str, "fsize") == 0) {
            limits[limit_index].resurce = RLIMIT_FSIZE;
            soft = strcmp(soft_str, "unlimited") == 0 ? RLIM_INFINITY : parse_size(soft_str);
            hard = strcmp(hard_str, "unlimited") == 0 ? RLIM_INFINITY : parse_size(hard_str);
        } else if (strcmp(res_str, "nofile") == 0) {
            limits[limit_index].resurce = RLIMIT_NOFILE;
            soft = strcmp(soft_str, "unlimited") == 0 ? RLIM_INFINITY : (rlim_t)atoi(soft_str);
            hard = strcmp(hard_str, "unlimited") == 0 ? RLIM_INFINITY : (rlim_t)atoi(hard_str);
        } else {
            fprintf(stderr, "Unknown resource type: %s\n", res_str);
            exit(1);
        }
        //limits[limit_index].resurce= resource;
        limits[limit_index].soft= soft;
        limits[limit_index].hard = hard;
        limit_index++;

    }
    return limit_index; // where the cmd starts
}

rlim_t parse_size(const char *str) {//will convert units
    char *endptr;
    double val = strtod(str, &endptr);
    while (isspace(*endptr)) endptr++;
    if (strcasecmp(endptr, "B") == 0 || *endptr == '\0') return (rlim_t)(unsigned long)val;
    if (strcasecmp(endptr, "K") == 0 || strcasecmp(endptr, "KB") == 0) return (rlim_t)(unsigned long)(val * 1024);
    if (strcasecmp(endptr, "M") == 0 || strcasecmp(endptr, "MB") == 0) return (rlim_t)(unsigned long)(val * 1024 * 1024);
    if (strcasecmp(endptr, "G") == 0 || strcasecmp(endptr, "GB") == 0) return (rlim_t)(unsigned long)(val * 1024 * 1024 * 1024);
    fprintf(stderr, "Invalid size suffix in: %s\n", str);
    exit(1);
}
void handle_rlimit_show_command() {//show the limits
    struct rlimit rl;

    getrlimit(RLIMIT_CPU, &rl);
    printf("cpu: soft=%ld, hard=%ld\n", rl.rlim_cur, rl.rlim_max);

    getrlimit(RLIMIT_AS, &rl);
    printf("mem: soft=%ld, hard=%ld\n", rl.rlim_cur, rl.rlim_max);

    getrlimit(RLIMIT_FSIZE, &rl);
    printf("fsize: soft=%ld, hard=%ld\n", rl.rlim_cur, rl.rlim_max);

    getrlimit(RLIMIT_NOFILE, &rl);
    printf("nofile: soft=%ld, hard=%ld\n", rl.rlim_cur, rl.rlim_max);
}
void apply_rlimits(LimitSpeac limits[]) {
    for (int j = 0; j < MAX_LIMITS; j++) {
        if (limits[j].resurce != 0) {
            struct rlimit rl;
            rl.rlim_cur = limits[j].soft;
            rl.rlim_max = limits[j].hard;
            if (setrlimit(limits[j].resurce, &rl) != 0) {
                perror("setrlimit failed");
                exit(1);
            }
        }
    }
}
void handler_sigchiled(int signum) {
    (void)signum;
    while (waitpid(-1, NULL, WNOHANG) >0) {

    }
}
void check_and_extract_stderr_redirect(char **args, int *count, char **filename, int *redirect_flag) {
    for (int i = 0; i < *count; i++) {
        // Check for both forms: "2>" or just number followed by ">"
        if (strcmp(args[i], "2>") == 0 ||
            (strlen(args[i]) > 1 && args[i][0] == '2' && args[i][1] == '>')) {

            if (strcmp(args[i], "2>") == 0) {
                // Format: "2>" "filename"
                if (i + 1 < *count) {
                    *filename = args[i + 1];
                    *redirect_flag = 1;

                    // Shift the rest of the args left by 2
                    for (int j = i; j + 2 <= *count; j++) {
                        args[j] = args[j + 2];
                    }

                    *count -= 2;
                    args[*count] = NULL;
                }
            } else {
                // Format: "2>filename"
                *filename = args[i] + 2; // Skip "2>"
                *redirect_flag = 1;

                // Shift the rest of the args left by 1
                for (int j = i; j + 1 <= *count; j++) {
                    args[j] = args[j + 1];
                }

                *count -= 1;
                args[*count] = NULL;
            }
            break;
            }
    }
}

int check_args_rlimit(char *str) {
    int count = 0;
    int in_word = 0;
    int r = 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '=')
            r++;
        if (str[i] != ' ' && in_word == 0) {
            in_word = 1;
            count++;
        } else if (str[i] == ' ') {
            in_word = 0;
        }
    }

    return count;
}
void handle_sigcpu(int signo)
{
    printf("CPU time limit exceeded!\n");
    exit(1);
}

void handle_sigfsz(int signo)
{
    printf("File size limit exceeded!\n");
    exit(1);
}

void handle_sigmem(int signo)
{
    printf("Memory allocation failed!\n");
    exit(1);
}

void handle_signof(int signo)
{
    printf("Too many open files!\n");
    exit(1);
}

int check_args_mcalc(char *str) {// check the args for mcalc
    int count = 0;
    int in_word = 0;
    int has_double_space = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ' ' && str[i + 1] == ' ') {
            has_double_space = 1;
        }
        if (!isspace(str[i]) && in_word == 0) {
            in_word = 1;
            count++;
        } else if (isspace(str[i])) {
            in_word = 0;
        }
    }
    if (isspace(str[0]) || isspace(str[strlen(str) - 1])) {
        has_double_space = 1;
    }
    if (has_double_space) {
        printf("ERR_MAT_INPUT\n");
        return -1;
    }
    return count;
}

McalcParsed parse_mcalc_struct_based(char **args, int count) {//sepearte the args for mcalc
    McalcParsed result;
    result.matrices = NULL;
    result.matrix_count = 0;
    result.operation = NULL;
    result.valid = 0;

    if (count < 2) {
        fprintf(stderr, "ERR: mcalc requires at least 2 matrices and an operation\n");
        return result;
    }

    char *operation = args[count - 1];
    if (strcmp(operation, "ADD") != 0 && strcmp(operation, "SUB") != 0) {
        fprintf(stderr, "ERR: Operation must be ADD or SUB (uppercase only)\n");
        return result;
    }

    int matrix_count = count - 1;
    result.matrices = malloc(sizeof(char *) * matrix_count);
    if (!result.matrices) {
        perror("malloc");
        return result;
    }

    for (int i = 0; i < matrix_count; i++) {
        result.matrices[i] = strdup(args[i]);
    }

    result.matrix_count = matrix_count;
    result.operation = strdup(operation);
    result.valid = 1;
    return result;
}
Matrix parse_matrix(const char *str) {// string to matrix
    Matrix m;
    m.rows = m.cols = 0;
    m.data = NULL;

    if (!str || str[0] != '(') {
        fprintf(stderr, "ERR_MAT_INPUT\n");
        return m;
    }

    char *copy = strdup(str + 1);
    char *token = strtok(copy, ":");
    if (!token) {
        fprintf(stderr, "ERR_MAT_INPUT\n");
        free(copy);
        return m;
    }

    if (sscanf(token, "%d,%d", &m.rows, &m.cols) != 2) {
        fprintf(stderr, "ERR_MAT_INPUT\n");
        free(copy);
        return m;
    }

    token = strtok(NULL, ")");
    if (!token) {
        fprintf(stderr, "ERR_MAT_INPUT\n");
        free(copy);
        return m;
    }

    int total = m.rows * m.cols;
    m.data = malloc(sizeof(double) * total);
    if (!m.data) {
        perror("malloc");
        free(copy);
        return m;
    }

    int i = 0;
    char *num = strtok(token, ",");
    while (num && i < total) {
        m.data[i++] = atof(num);
        num = strtok(NULL, ",");
    }

    if (i != total) {
        fprintf(stderr, "ERR_MAT_INPUT\n");
        free(m.data);
        m.data = NULL;
    }

    free(copy);
    return m;
}

// Function to add two matrices of the same dimensions
Matrix matrix_add(const Matrix *a, const Matrix *b) {
    if (!a || !b || a->rows != b->rows || a->cols != b->cols) {
        printf("ERR_MAT_INPUT\n");
        exit(1);
    }

    // Initialize result matrix
    Matrix result;
    result.rows = a->rows;
    result.cols = a->cols;
    int total = result.rows * result.cols;

    // Allocate memory for result matrix data
    result.data = malloc(sizeof(double) * total);
    if (!result.data) {
        perror("malloc");
        exit(1);
    }

    // Perform element-wise addition
    for (int i = 0; i < total; i++) {
        result.data[i] = a->data[i] + b->data[i];
    }

    return result; // Return the new result matrix
}

Matrix matrix_sub(const Matrix *a, const Matrix *b) {
    if (!a || !b || a->rows != b->rows || a->cols != b->cols) {
        printf("ERR_MAT_INPUT\n");
        exit(1);
    }

    // Initialize result matrix
    Matrix result;
    result.rows = a->rows;
    result.cols = a->cols;
    int total = result.rows * result.cols;

    // Allocate memory for result matrix data
    result.data = malloc(sizeof(double) * total);
    if (!result.data) {
        perror("malloc");
        exit(1);
    }

    // Perform element-wise subtraction
    for (int i = 0; i < total; i++) {
        result.data[i] = a->data[i] - b->data[i];
    }

    return result; // Return the new result matrix
}
Matrix run_parallel_matrix_calc(Matrix *matrices, int count, const char *operation) {
    Matrix **current_level = malloc(sizeof(Matrix *) * count);
    if (!current_level) {
        perror("malloc");
        exit(1);
    }

    // Copy data from matrices to current_level before freeing matrices
    for (int i = 0; i < count; i++) {
        current_level[i] = malloc(sizeof(Matrix));
        if (!current_level[i]) {
            perror("malloc");
            exit(1);
        }

        current_level[i]->rows = matrices[i].rows;
        current_level[i]->cols = matrices[i].cols;
        int total = matrices[i].rows * matrices[i].cols;

        current_level[i]->data = malloc(sizeof(double) * total);
        if (!current_level[i]->data) {
            perror("malloc");
            exit(1);
        }

        for (int j = 0; j < total; j++) {
            current_level[i]->data[j] = matrices[i].data[j];
        }
    }

    // Now it's safe to free matrices and their data
    for (int i = 0; i < count; i++) {
        free(matrices[i].data);
    }
    //free(matrices);

    while (count > 1) {
        int pair_count = count / 2;
        int next_count = (count % 2 == 0) ? pair_count : pair_count + 1;

        Matrix **next_level = malloc(sizeof(Matrix *) * next_count);
        pthread_t *threads = malloc(sizeof(pthread_t) * pair_count);
        void ***args = malloc(sizeof(void **) * pair_count);

        for (int i = 0; i < pair_count; i++) {
            args[i] = malloc(sizeof(void *) * 3);
            args[i][0] = current_level[i * 2];
            args[i][1] = current_level[i * 2 + 1];
            args[i][2] = (void *)operation;

            pthread_create(&threads[i], NULL, thread_matrix_op, args[i]);
        }

        for (int i = 0; i < pair_count; i++) {
            Matrix *res;
            pthread_join(threads[i], (void **)&res);
            next_level[i] = res;

            free(current_level[i * 2]->data);
            free(current_level[i * 2]);
            free(current_level[i * 2 + 1]->data);
            free(current_level[i * 2 + 1]);
            free(args[i]);
        }

        if (count % 2 == 1) {
            next_level[next_count - 1] = current_level[count - 1];
        }

        free(current_level);
        free(threads);
        free(args);

        current_level = next_level;
        count = next_count;
    }

    Matrix *final = current_level[0];
    Matrix result;
    result.rows = final->rows;
    result.cols = final->cols;
    int total = result.rows * result.cols;

    result.data = malloc(sizeof(double) * total);
    if (!result.data) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < total; i++) {
        result.data[i] = final->data[i];
    }

    free(final->data);
    free(final);
    free(current_level);

    return result;
}


void *thread_matrix_op(void *arg) {// thread function to handle matrix operations
    if (!arg) {
        fprintf(stderr, "ERR_MAT_INPUT\n");
        exit(1);
    }
    void **args = (void **)arg;
    Matrix *a = (Matrix *)args[0];
    Matrix *b = (Matrix *)args[1];
    char *op = (char *)args[2];

    Matrix *res = malloc(sizeof(Matrix));
    if (!res) {
        perror("malloc");
        exit(1);
    }

    if (strcmp(op, "ADD") == 0) {
        *res = matrix_add(a, b);
    } else if (strcmp(op, "SUB") == 0) {
        *res = matrix_sub(a, b);
    } else {
        fprintf(stderr, "ERR: Unknown operation\n");
        exit(1);
    }

    return res;
}
// Utility function to free parsed mcalc arguments
void free_mcalc_parsed(McalcParsed *parsed) {
    if (parsed->matrices) {
        for (int i = 0; i < parsed->matrix_count; i++) {
            free(parsed->matrices[i]);
        }
        free(parsed->matrices);
        parsed->matrices = NULL;
    }
    if (parsed->operation) {
        free(parsed->operation);
        parsed->operation = NULL;
    }
    parsed->matrix_count = 0;
    parsed->valid = 0;
}

// Utility function to free matrix array
void free_matrix_array(Matrix *matrices, int count, bool free_data) {
    if (!matrices) return;
    for (int i = 0; i < count; i++) {
        if (free_data && matrices[i].data) {
            free(matrices[i].data);
            matrices[i].data = NULL;
        }
    }
    free(matrices);
}
