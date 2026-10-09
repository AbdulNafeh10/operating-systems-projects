#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("Small sample program for Shelf Steam");
    } else if (argc == 2 && strcmp(argv[1], "stdin") == 0) {
        char text[80];
        if (fgets(text, sizeof(text), stdin)) printf("Input: %s", text);
    } else {
        for (int i = 1; i < argc; i++) printf("Argument: %s\n", argv[i]);
    }
    return 0;
}
