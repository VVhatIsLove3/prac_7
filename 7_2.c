#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <limits.h>

#define DEFAULT_DIR "~/files"
#define INITIAL_BUF_SIZE 1024

int case_insensitive = 0;

// Функция для вывода атрибутов файла
void print_file_attributes(const char *filename, const struct stat *stat_buf) {
    // Права доступа в численном формате
    mode_t mode = stat_buf->st_mode & 0777;
    
    // Получаем информацию о владельце
    struct passwd *pw = getpwuid(stat_buf->st_uid);
    char *owner = "unknown";
    if (pw != NULL) {
        owner = pw->pw_name;
    }
    
    // Получаем информацию о группе
    struct group *gr = getgrgid(stat_buf->st_gid);
    char *group = "unknown";
    if (gr != NULL) {
        group = gr->gr_name;
    }
    
    // Размер файла
    off_t size = stat_buf->st_size;
    
    fprintf(stderr, "Атрибуты файла '%s':\n", filename);
    fprintf(stderr, "  Права доступа: %o\n", mode);
    fprintf(stderr, "  Владелец: %s\n", owner);
    fprintf(stderr, "  Группа: %s\n", group);
    fprintf(stderr, "  Размер: %ld байт\n", (long)size);
}

// Функция для сравнения строк с учетом/без учета регистра
int str_case_cmp(const char *s1, const char *s2, size_t n) {
    if (case_insensitive) {
        return strncasecmp(s1, s2, n);
    } else {
        return strncmp(s1, s2, n);
    }
}

// Функция для чтения строки произвольной длины
char* read_long_line(FILE *file, char **buffer, size_t *buf_size) {
    size_t pos = 0;
    (*buffer)[0] = '\0';

    while (fgets(*buffer + pos, *buf_size - pos, file)) {
        pos += strlen(*buffer + pos);
        
        // Если строка полностью прочитана (завершается \n или EOF)
        if ((pos > 0 && (*buffer)[pos-1] == '\n') || feof(file)) {
            return *buffer;
        }
        
        // Увеличиваем буфер в 2 раза
        size_t new_size = *buf_size * 2;
        char *new_buf = realloc(*buffer, new_size);
        if (!new_buf) {
            perror("realloc failed");
            free(*buffer);
            return NULL;
        }
        *buffer = new_buf;
        *buf_size = new_size;
    }
    
    return NULL; // Ошибка чтения или EOF
}

void search_in_file(const char *filename, const char *search_word) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        struct stat stat_buf;
        if (lstat(filename, &stat_buf) == -1) {
            fprintf(stderr, "Ошибка получения информации о файле '%s': %s\n", filename, strerror(errno));
            return;
        }

        fprintf(stderr, "Ошибка открытия файла '%s': %s\n", filename, strerror(errno));
        print_file_attributes(filename, &stat_buf);
        return;
    }

    size_t buf_size = INITIAL_BUF_SIZE;
    char *line = malloc(buf_size);
    if (!line) {
        perror("malloc failed");
        fclose(file);
        return;
    }

    int line_number = 1;
    int matches_found = 0;
    size_t word_len = strlen(search_word);

    while (read_long_line(file, &line, &buf_size)) {
        char *pos = line;
        while (*pos != '\0') {
            if (str_case_cmp(pos, search_word, word_len) == 0) {
                // Проверяем границы слова
                int is_word_start = (pos == line) || isspace(pos[-1]) || ispunct(pos[-1]);
                char next_char = pos[word_len];
                int is_word_end = isspace(next_char) || ispunct(next_char) || next_char == '\0';
                
                if (is_word_start && is_word_end) {
                    if (!matches_found) {
                        printf("\nФайл: %s\n", filename);
                        matches_found = 1;
                    }
                    printf("Строка %d: %s", line_number, line);
                    break;
                }
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
    if (!dir) {
        fprintf(stderr, "Ошибка открытия директории '%s': %s\n", dir_path, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        if (snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name) >= sizeof(full_path)) {
            fprintf(stderr, "Слишком длинный путь: %s/%s\n", dir_path, entry->d_name);
            continue;
        }

        struct stat stat_buf;
        if (lstat(full_path, &stat_buf) == -1) {
            fprintf(stderr, "Ошибка доступа к '%s': %s\n", full_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(stat_buf.st_mode)) {
            search_directory(full_path, search_word);
        } else if (S_ISREG(stat_buf.st_mode)) {
            search_in_file(full_path, search_word);
        }
    }

    closedir(dir);
}

char *expand_home_dir(const char *path) {
    if (!path) {
        return strdup(".");
    }
    if (path[0] != '~') {
        return strdup(path);
    }

    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw) {
            perror("getpwuid failed");
            return strdup(".");
        }
        home = pw->pw_dir;
    }

    char *expanded = malloc(strlen(home) + strlen(path) + 1);
    if (!expanded) {
        perror("malloc failed");
        return strdup(".");
    }

    sprintf(expanded, "%s%s", home, path + 1);
    return expanded;
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "Использование: %s [-i] [ДИРЕКТОРИЯ] <СЛОВО>\n", prog_name);
    fprintf(stderr, "Или: %s [-i] <СЛОВО> [ДИРЕКТОРИЯ]\n", prog_name);
    fprintf(stderr, "Опции:\n");
    fprintf(stderr, "  -i      Поиск без учета регистра\n");
    fprintf(stderr, "По умолчанию директория: %s\n", DEFAULT_DIR);
}

int is_directory(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return S_ISDIR(stat_buf.st_mode);
}

int main(int argc, char *argv[]) {
    char *search_dir = DEFAULT_DIR;
    char *search_word = NULL;
    int dir_arg_index = 1;

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Обработка опции -i
    if (strcmp(argv[1], "-i") == 0) {
        case_insensitive = 1;
        dir_arg_index = 2;
        if (argc < 3) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // Определение директории и слова для поиска
    if (argc > dir_arg_index + 1) {
        // Проверяем, какой из аргументов является директорией
        if (is_directory(argv[dir_arg_index])) {
            search_dir = argv[dir_arg_index];
            search_word = argv[dir_arg_index + 1];
        } else if (is_directory(argv[dir_arg_index + 1])) {
            search_word = argv[dir_arg_index];
            search_dir = argv[dir_arg_index + 1];
        } else {
            // Если ни один из аргументов не является директорией, считаем первый словом
            search_word = argv[dir_arg_index];
        }
    } else {
        search_word = argv[dir_arg_index];
    }

    char *expanded_dir = expand_home_dir(search_dir);
    if (!expanded_dir) {
        fprintf(stderr, "Ошибка обработки пути\n");
        return EXIT_FAILURE;
    }

    printf("Поиск '%s' в директории: %s (", search_word, expanded_dir);
    if (case_insensitive) {
        printf("без учета регистра");
    } else {
        printf("с учетом регистра");
    }
    printf(")\n");
    
    search_directory(expanded_dir, search_word);

    free(expanded_dir);
    return 0;
}
