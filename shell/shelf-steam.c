#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_ARGS 20

static void error(void) {
    fputs("An error has occurred\n", stderr);
}

static int compare_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static char *full_path(const char *dir, const char *name) {
    size_t length = strlen(dir) + strlen(name) + 2;
    char *path = malloc(length);
    if (path) snprintf(path, length, "%s/%s", dir, name);
    return path;
}

static int is_executable(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode) && access(path, X_OK) == 0;
}

static void description(const char *path, char *text, size_t size) {
    snprintf(text, size, "(empty)");
    if (!is_executable(path)) return;

    FILE *binary = fopen(path, "rb");
    if (!binary) return;
    unsigned char magic[4];
    int elf = fread(magic, 1, sizeof(magic), binary) == sizeof(magic) &&
              memcmp(magic, "\177ELF", sizeof(magic)) == 0;
    fclose(binary);
    if (!elf) return;

    // An anonymous temporary file avoids collisions between shell instances.
    FILE *output = tmpfile();
    if (!output) return;
    pid_t pid = fork();
    if (pid == 0) {
        if (dup2(fileno(output), STDOUT_FILENO) == -1) _exit(127);
        fclose(output);
        execl(path, path, "--help", (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        int status;
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
        rewind(output);
        if (fgets(text, (int)size, output)) {
            text[strcspn(text, "\r\n")] = '\0';
            if (!text[0]) snprintf(text, size, "(empty)");
        }
    }
    fclose(output);
}

static void list_programs(const char *dir) {
    DIR *directory = opendir(dir);
    if (!directory) { error(); return; }

    char **names = NULL;
    size_t count = 0, capacity = 0;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (count == capacity) {
            size_t next = capacity ? capacity * 2 : 16;
            char **tmp = realloc(names, next * sizeof(*names));
            if (!tmp) { error(); break; }
            names = tmp;
            capacity = next;
        }
        names[count] = strdup(entry->d_name);
        if (!names[count]) { error(); break; }
        count++;
    }
    closedir(directory);

    qsort(names, count, sizeof(*names), compare_names);
    for (size_t i = 0; i < count; i++) {
        char *path = full_path(dir, names[i]);
        char text[256] = "(empty)";
        if (path) description(path, text, sizeof(text));
        else error();
        printf("%s: %s\n", names[i], text);
        free(path);
        free(names[i]);
    }
    free(names);
}

static int valid_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int main(int argc, char **argv) {
    if (argc != 2 || !valid_dir(argv[1])) { error(); return 1; }
    char *dir = strdup(argv[1]);
    if (!dir) { error(); return 1; }

    char *line = NULL;
    size_t length = 0;
    while (1) {
        fputs("shelf-steam> ", stdout);
        fflush(stdout);
        if (getline(&line, &length, stdin) == -1) break;

        char *args[MAX_ARGS + 1];
        int count = 0, too_many = 0;
        char *save = NULL;
        for (char *part = strtok_r(line, " \t\r\n", &save); part;
             part = strtok_r(NULL, " \t\r\n", &save)) {
            if (count == MAX_ARGS) { too_many = 1; break; }
            args[count++] = part;
        }
        if (!count) continue;
        if (too_many) { error(); continue; }
        args[count] = NULL;

        if (!strcmp(args[0], "exit")) {
            if (count != 1) { error(); continue; }
            break;
        }
        if (!strcmp(args[0], "path")) {
            if (count != 2 || !valid_dir(args[1])) { error(); continue; }
            char *next = strdup(args[1]);
            if (!next) { error(); continue; }
            free(dir);
            dir = next;
            continue;
        }
        if (!strcmp(args[0], "ls")) {
            if (count != 1) error();
            else list_programs(dir);
            continue;
        }

        int input_index = -1;
        for (int i = 1; i < count; i++) {
            if (!strcmp(args[i], "<")) {
                if (input_index != -1) { input_index = -2; break; }
                input_index = i;
            }
        }
        if (input_index == -2 || (input_index >= 0 && input_index != count - 2)) {
            error();
            continue;
        }
        char *input_file = NULL;
        if (input_index >= 0) {
            input_file = args[count - 1];
            args[input_index] = NULL;
        }
        char *path = full_path(dir, args[0]);
        if (!path || !is_executable(path)) {
            error();
            free(path);
            continue;
        }
        pid_t pid = fork();
        if (pid == 0) {
            if (input_file) {
                int fd = open(input_file, O_RDONLY);
                if (fd == -1 || dup2(fd, STDIN_FILENO) == -1) {
                    error();
                    _exit(1);
                }
                close(fd);
            }
            // exec replaces the child, keeping the shell alive in the parent.
            execv(path, args);
            error();
            _exit(1);
        }
        if (pid == -1) error();
        else {
            int status;
            while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
        }
        free(path);
    }
    free(line);
    free(dir);
    return 0;
}
