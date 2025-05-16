#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>
#include <ctype.h>
#include <grp.h>

#define DEFAULT_DIR "~/files"
#define INITIAL_BUF_SIZE 128

// Функция для проверки, является ли путь директорией
int is_directory(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return S_ISDIR(stat_buf.st_mode);
}

// Функция для вывода атрибутов файла
void print_file_attributes(const char *filename, const struct stat *stat_buf) {
    mode_t mode = stat_buf->st_mode & 0777;
    
    const char *owner = "unknown";
    struct passwd *pw = getpwuid(stat_buf->st_uid);
    if (pw != NULL && pw->pw_name != NULL) {
        owner = pw->pw_name;
    }
    
    const char *group = "unknown";
    struct group *gr = getgrgid(stat_buf->st_gid);
    if (gr != NULL && gr->gr_name != NULL) {
        group = gr->gr_name;
    }
    
    fprintf(stderr, "Атрибуты файла '%s':\n", filename);
    fprintf(stderr, "  Права доступа: %03o\n", mode);
    fprintf(stderr, "  Владелец: %s\n", owner);
    fprintf(stderr, "  Группа: %s\n", group);
    fprintf(stderr, "  Размер: %ld байт\n", (long)stat_buf->st_size);
}

void search_in_file(const char *filename, const struct stat *stat_buf, const char *search_word) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Файл не открылся (пу-пу-пу) %s: %s\n", filename, strerror(errno));
        print_file_attributes(filename, stat_buf);
        return;
    }

    size_t buf_size = INITIAL_BUF_SIZE;
    char *line = malloc(buf_size);
    if (line == NULL) {
        fclose(file);
        fprintf(stderr, "Ошибка выделения памяти\n");
        return;
    }

    int line_number = 1;
    int found = 0;

    while (1) {
        if (fgets(line, buf_size, file) == NULL) {
            break;
        }

        size_t len = strlen(line);
        while (len == buf_size - 1 && line[len - 1] != '\n') {
            buf_size *= 2;
            char *new_line = realloc(line, buf_size);
            if (new_line == NULL) {
                free(line);
                fclose(file);
                fprintf(stderr, "Ошибка выделения памяти\n");
                return;
            }
            line = new_line;
            
            if (fgets(line + len, buf_size - len, file) == NULL) {
                break;
            }
            len += strlen(line + len);
        }

        char *pos = line;
        while ((pos = strstr(pos, search_word)) != NULL) {
            int word_start = (pos == line) || 
                           isspace((unsigned char)pos[-1]) || 
                           ispunct((unsigned char)pos[-1]);

            char next_char = pos[strlen(search_word)];
            int word_end = isspace((unsigned char)next_char) || 
                         ispunct((unsigned char)next_char) || 
                         next_char == '\0';

            if (word_start && word_end) {
                if (!found) {
                    printf("\nФайл: %s\n", filename);
                    found = 1;
                }
                printf("Строка %d: %s", line_number, line);
                break;
            }
            pos++;
        }
        line_number++;
    }

    free(line);
    fclose(file);
}

void search_directory(const char *dir_path, const char *search_word) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        fprintf(stderr, "Ошибка открытия директории: %s: %s\n", dir_path, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat stat_buf;
        if (lstat(full_path, &stat_buf) == -1) {
            fprintf(stderr, "Ошибка входа внутрь %s: %s\n", full_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(stat_buf.st_mode)) {
            search_directory(full_path, search_word);
        } else if (S_ISREG(stat_buf.st_mode)) {
            search_in_file(full_path, &stat_buf, search_word);
        }
    }

    closedir(dir);
}

char *expand_home_dir(const char *path) {
    if (path[0] != '~') {
        return strdup(path);
    }

    const char *home = getenv("HOME");
    if (home == NULL) {
        struct passwd *pw = getpwuid(getuid());
        if (pw == NULL) {
            perror("getpwuid failed");
            return strdup(path);
        }
        home = pw->pw_dir;
    }

    size_t len = strlen(home) + strlen(path);
    char *expanded = malloc(len + 1);
    if (expanded == NULL) {
        perror("malloc failed");
        return strdup(path);
    }

    strcpy(expanded, home);
    strcat(expanded, path + 1);
    return expanded;
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "Использование: %s [ДИРЕКТОРИЯ] <СЛОВО>\n", prog_name);
    fprintf(stderr, "Или: %s <СЛОВО> [ДИРЕКТОРИЯ]\n", prog_name);
    fprintf(stderr, "Если директория не задана, используется: %s\n", DEFAULT_DIR);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    char *search_dir = DEFAULT_DIR;
    char *search_word = NULL;
    int dir_specified = 0;

    // Обработка аргументов
    for (int i = 1; i < argc; i++) {
        if (is_directory(argv[i])) {
            search_dir = argv[i];
            dir_specified = 1;
        } else {
            search_word = argv[i];
        }
    }

    if (search_word == NULL) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    char *expanded_dir = expand_home_dir(search_dir);
    if (expanded_dir == NULL) {
        fprintf(stderr, "Ошибка обработки пути\n");
        return EXIT_FAILURE;
    }

    printf("Поиск '%s' в директории: %s\n", search_word, expanded_dir);

    search_directory(expanded_dir, search_word);

    free(expanded_dir);
    return 0;
}
