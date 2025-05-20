//
// Created by student on 4/7/25.
//
#define MAXSIZE 1025
#define MAXARG 7
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
typedef struct {
    char *dang_cmd;//the dangerus cmd
} DangerCommand;
int check_args(char *args);
void splitWords(char ***words, char* args, int count);
void terminal (char* danger_name, char *log_name) ;
double calc_time (struct timespec start, struct timespec end);
void min_max_sec (double *min, double *max, double curr);
int load_dangerous_commands(char *filename, DangerCommand **danger_list);
int check_if_blocked_command(char **args, int count_arg, DangerCommand *danger_list, int danger_count, int *blocked_count);
void free_dangerous_commands(DangerCommand *danger_list, int danger_count);
int count_lines(char *name);


void terminal (char* danger_name, char *log_name) {
    FILE *fp = fopen((log_name), "a");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    char** words;
    pid_t pid;
    double min =0 , max= 0 , avg = 0, last_cmd =0;
    int counter_success = 0 , dangerous_comand =0;
    char str[MAXSIZE];
    DangerCommand *danger_list;
    int danger_count = load_dangerous_commands(danger_name, &danger_list);
    printf("#cmd:%d|#dangerous_cmd_blocked:%d|#last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time:%.5f>>",counter_success,dangerous_comand,last_cmd,avg,min,max);
    fgets (str, MAXSIZE, stdin);
    str[strcspn(str, "\n")] = '\0';
    while (strcmp(str, "done")!= 0) {
        int count = check_args(str);
        if (count != -1) {//runs the order
                 struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start);

                splitWords(&words, str, count);
                int is_blocked = check_if_blocked_command(words, count, danger_list, danger_count, &dangerous_comand);
                if (is_blocked) {
                    for (int i = 0; i < count; i++) free(words[i]);
                    free(words);
                    printf("#cmd:%d|#dangerous_cmd_blocked:%d|#last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time:%.5f>>",
                       counter_success, dangerous_comand, last_cmd, avg, min, max);
                    fgets(str, MAXSIZE, stdin);
                    str[strcspn(str, "\n")] = '\0';
                    continue;
                }

                pid = fork();
                if (pid < 0) {
                    perror("fork");
                }
                if (pid == 0) {
                    execvp(words[0], words);
                    perror("execvp");
                    exit(255);
                }
                int status;
                wait(&status);
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    counter_success++;
                    clock_gettime(CLOCK_MONOTONIC, &end);
                    last_cmd = calc_time(start, end);
                    min_max_sec(&min, &max, last_cmd);
                    avg= ((avg * (counter_success-1)) + last_cmd) / counter_success;
                    fprintf(fp,"%s : %.5f sec \n" , str , last_cmd);
                    fflush(fp);
                }
                for (int i = 0; i < count; i++) {// free the memory of the child
                    free(words[i]);
                }
                free(words);
            }
            printf("#cmd:%d|#dangerous_cmd_blocked:%d|#last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time:%.5f>>",counter_success,dangerous_comand,last_cmd,avg,min,max);
            fgets (str, MAXSIZE, stdin);
            str[strcspn(str, "\n")] = '\0';//maks sure that we get a new input
        }
    printf("%d \n", dangerous_comand);
    free_dangerous_commands(danger_list, danger_count);
    fclose(fp);
    }


int check_args(char *args) {
    int count = 0;
    if (args == NULL) {
        return -1;
    }
    for (int i = 1; args[i] != '\0'; i++) {//check if there is doubel spase
        if (isspace(args[i]) && isspace(args[i - 1])) {
            printf("ERR_SPACE\n");
            return -1;
        }
        if (isspace(args[i - 1]) && !isspace(args[i])) {//count each word
            count++;
        }
    }
    if (args[0] != '\0' && !isspace(args[0])) {//adds the first word
        count++;
    }
    //check if there is more then 6 args
    if (count > MAXARG) {
        printf("ERR_ARGS\n");
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

    int word_index = 0;
    char temp[MAXSIZE];
    strcpy(temp, args);

    char* token = strtok(temp, " ");
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
int load_dangerous_commands(char *filename, DangerCommand **danger_list) {// gets the file name, arry to comands , max size for arry
    int max_size = count_lines(filename);
    if (max_size <= 0) {
        fprintf(stderr, "No dangerous commands loaded.\n");
        return 0;
    }

    *danger_list = (DangerCommand*)malloc(max_size * sizeof(DangerCommand));

    FILE *dangerous_commands = fopen(filename, "r");
    if (!dangerous_commands) {
        perror("fopen");
        return -1;  //
    }
    char line[MAXSIZE];
    int danger_count = 0;

    while (fgets(line, sizeof(line), dangerous_commands)) {
        line[strcspn(line, "\n")] = '\0'; // removes newline
        (*danger_list)[danger_count].dang_cmd = strdup(line);
        danger_count++;
    }

    fclose(dangerous_commands);
    return danger_count;  //
}
int check_if_blocked_command(char **args, int count_arg, DangerCommand *danger_list, int danger_count, int *blocked_count) {
    // the comand after "split words", how meny args, the dangeros cmd after "load", the num that returns, how meny blocked so far
    char full_cmd[MAXSIZE] = "";
    for (int i = 0; i < count_arg; i++) {
        strcat(full_cmd, args[i]);
        if (i != count_arg - 1) strcat(full_cmd, " ");// copys the user cmd
    }

    for (int i = 0; i < danger_count; i++) {
        if (strcmp(full_cmd, danger_list[i].dang_cmd) == 0) {
            printf("ERR: Dangerous command detected (\"%s\"). Execution prevented.\n", full_cmd);
            (*blocked_count)++;
            return 1;
        } else {
            char temp[MAXSIZE];// to do tok
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
        danger_list[i].dang_cmd = NULL; //
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
