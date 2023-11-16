#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include <libgen.h>
#include <errno.h>

typedef struct RGB
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB;

typedef struct BMPHeader
{
    uint32_t size;
    int32_t width_px;
    int32_t height_px;
    uint32_t image_size_bytes;
    RGB rgb;
} BMPHeader;

#define BMP_SIZE_OFFSET 2
#define BMP_WIDTH_OFFSET 18
#define BMP_HEIGHT_OFFSET 22
#define BMP_IMAGE_SIZE_OFFSET 34
#define BMP_HEADER_SIZE 54

void read_bmp_header(int fd, BMPHeader *bmp_header)
{
    char buffer[54];
    if (lseek(fd, BMP_SIZE_OFFSET, SEEK_SET) == -1)
    {
        perror("Eroare la lseek");
        exit(EXIT_FAILURE);
    }
    if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer))
    {
        perror("Eroare la citirea header-ului BMP");
        exit(EXIT_FAILURE);
    }

    bmp_header->size = *((uint32_t *)&buffer[BMP_SIZE_OFFSET - BMP_SIZE_OFFSET]);
    bmp_header->width_px = *((int32_t *)&buffer[BMP_WIDTH_OFFSET - BMP_SIZE_OFFSET]);
    bmp_header->height_px = *((int32_t *)&buffer[BMP_HEIGHT_OFFSET - BMP_SIZE_OFFSET]);
    bmp_header->image_size_bytes = *((uint32_t *)&buffer[BMP_IMAGE_SIZE_OFFSET - BMP_SIZE_OFFSET]);
}

void read_bmp_pixel_data(int fd, BMPHeader *bmp_header, RGB **pixels)
{
    *pixels = malloc(bmp_header->image_size_bytes);
    if (*pixels == NULL)
    {
        perror("Eroare la alocarea mem pt pixeli");
        exit(EXIT_FAILURE);
    }

    if (lseek(fd, BMP_HEADER_SIZE, SEEK_SET) == -1)
    {
        perror("Eroare in lseek");
        free(*pixels);
        exit(EXIT_FAILURE);
    }

    if (read(fd, *pixels, bmp_header->image_size_bytes) != bmp_header->image_size_bytes)
    {
        perror("Eroare la citirea pixelilor");
        free(*pixels);
        exit(EXIT_FAILURE);
    }
}

void convert_bmp(int fd, BMPHeader *bmp_header, RGB *pixels)
{
    uint8_t gray;
    int padding = (4 - (bmp_header->width_px * sizeof(RGB)) % 4) % 4;

    for (int y = 0; y < bmp_header->height_px; y++)
    {
        for (int x = 0; x < bmp_header->width_px; x++)
        {
            int index = y * (bmp_header->width_px * sizeof(RGB) + padding) + x * sizeof(RGB);

            gray = (uint8_t)(pixels[index / sizeof(RGB)].red * 0.299 +
                             pixels[index / sizeof(RGB)].green * 0.587 +
                             pixels[index / sizeof(RGB)].blue * 0.114);
            pixels[index / sizeof(RGB)].red = gray;
            pixels[index / sizeof(RGB)].green = gray;
            pixels[index / sizeof(RGB)].blue = gray;
        }
    }
}

void write_pixel_data(int bmp_file, BMPHeader *bmp_header, RGB *pixels)
{
    if (lseek(bmp_file, BMP_HEADER_SIZE, SEEK_SET) == -1)
    {
        perror("Error seeking to pixel data");
        exit(EXIT_FAILURE);
    }

    if (write(bmp_file, pixels, bmp_header->image_size_bytes) != bmp_header->image_size_bytes)
    {
        perror("Error writing pixel data");
        exit(EXIT_FAILURE);
    }
}

void convert(char *full_path)
{
    int bmp_file;
    RGB *pixels = NULL;
    BMPHeader bmp_header;

    bmp_file = open(full_path, O_RDWR);

    if (bmp_file == -1)
    {
        perror("Eroare la deschiderea fisierului BMP.");
        exit(EXIT_FAILURE);
    }
    read_bmp_header(bmp_file, &bmp_header);
    read_bmp_pixel_data(bmp_file, &bmp_header, &pixels);
    convert_bmp(bmp_file, &bmp_header, pixels);
    write_pixel_data(bmp_file, &bmp_header, pixels);
}

void write_bmp_info(const char *filename, char *input_dir, char *output_dir, int fd_statistica)
{
    BMPHeader bmp_header;
    struct stat bmp_stat;
    struct tm *mod_time;
    char buffer[1024];
    int written, bmp_file;
    char full_path[2048];

    memset(full_path, 0, sizeof(full_path));

    snprintf(full_path, sizeof(full_path), "%s/%s", input_dir, filename);

    bmp_file = open(full_path, O_RDONLY);

    if (bmp_file == -1)
    {
        perror("Eroare la deschiderea fisierului BMP.");
        exit(EXIT_FAILURE);
    }

    read_bmp_header(bmp_file, &bmp_header);
    fstat(bmp_file, &bmp_stat);
    close(bmp_file);

    mod_time = localtime(&bmp_stat.st_mtime);

    char time_str[11];
    strftime(time_str, sizeof(time_str), "%d.%m.%Y", mod_time);

    written = snprintf(buffer, sizeof(buffer),
                       "nume fisier: %s\ninaltime: %d\nlungime: %d\ndimensiune: %ld\n"
                       "identificatorul utilizatorului: %d\ntimpul ultimei modificari: %s\n"
                       "contorul de legaturi: %ld\ndrepturi de acces user: %c%c%c\n"
                       "drepturi de acces grup: %c%c%c\ndrepturi de acces altii: %c%c%c\n\n\n",
                       filename,
                       bmp_header.height_px,
                       bmp_header.width_px,
                       bmp_stat.st_size,
                       bmp_stat.st_uid,
                       time_str,
                       bmp_stat.st_nlink,
                       (bmp_stat.st_mode & S_IRUSR) ? 'R' : '-',
                       (bmp_stat.st_mode & S_IWUSR) ? 'W' : '-',
                       (bmp_stat.st_mode & S_IXUSR) ? 'X' : '-',
                       (bmp_stat.st_mode & S_IRGRP) ? 'R' : '-',
                       (bmp_stat.st_mode & S_IWGRP) ? 'W' : '-',
                       (bmp_stat.st_mode & S_IXGRP) ? 'X' : '-',
                       (bmp_stat.st_mode & S_IROTH) ? 'R' : '-',
                       (bmp_stat.st_mode & S_IWOTH) ? 'W' : '-',
                       (bmp_stat.st_mode & S_IXOTH) ? 'X' : '-');

    if (written > 0)
    {
        write(fd_statistica, buffer, written);
    }
}

int is_bmp(char *filename, char *input_dir)
{
    const char *ext = strrchr(filename, '.');

    if (ext && strcmp(ext, ".bmp") == 0)
        return 1;
    else
        return 0;
}

void write_statistica(char *filename, char *input_dir, char *output_dir, struct stat *status, int fd_statistica)
{
    char buffer[1024];
    int written;
    struct tm *mod_time;
    char full_path[257];
    snprintf(full_path, sizeof(full_path), "%s/%s", input_dir, filename);

    if (S_ISLNK(status->st_mode))
    {
        struct stat target_status;
        if (stat(full_path, &target_status) == -1)
        {
            perror("Eroare la status");
            exit(EXIT_FAILURE);
        }
        written = sprintf(buffer, "nume legatura: %s\ndimensiune legatura: %ld\n"
                                  "dimensiune fisier: %ld\ndrepturi de acces user legatura: %c%c%c\n",
                          filename, status->st_size, target_status.st_size,
                          (status->st_mode & S_IRUSR) ? 'R' : '-',
                          (status->st_mode & S_IWUSR) ? 'W' : '-',
                          (status->st_mode & S_IXUSR) ? 'X' : '-');

        if (written > 0)
        {
            write(fd_statistica, buffer, written);
        }
    }
    if (S_ISREG(status->st_mode))
    {

        mod_time = localtime(&status->st_mtime);
        char time_str[11];
        strftime(time_str, sizeof(time_str), "%d.%m.%Y", mod_time);
        const char *ext = strrchr(filename, '.');

        if (ext && strcmp(ext, ".bmp") == 0)
        {
            write_bmp_info(filename, input_dir, output_dir, fd_statistica);
        }
        else
        {
            written = snprintf(buffer, sizeof(buffer),
                               "nume fisier: %s\ndimensiune: %ld\n"
                               "identificatorul utilizatorului: %d\ntimpul ultimei modificari: %s\n"
                               "contorul de legaturi: %ld\ndrepturi de acces user: %c%c%c\n"
                               "drepturi de acces grup: %c%c%c\ndrepturi de acces altii: %c%c%c\n\n\n",
                               filename,
                               status->st_size,
                               status->st_uid,
                               time_str,
                               status->st_nlink,
                               (status->st_mode & S_IRUSR) ? 'R' : '-',
                               (status->st_mode & S_IWUSR) ? 'W' : '-',
                               (status->st_mode & S_IXUSR) ? 'X' : '-',
                               (status->st_mode & S_IRGRP) ? 'R' : '-',
                               (status->st_mode & S_IWGRP) ? 'W' : '-',
                               (status->st_mode & S_IXGRP) ? 'X' : '-',
                               (status->st_mode & S_IROTH) ? 'R' : '-',
                               (status->st_mode & S_IWOTH) ? 'W' : '-',
                               (status->st_mode & S_IXOTH) ? 'X' : '-');

            if (written > 0)
            {
                write(fd_statistica, buffer, written);
            }
        }
    }
    else if (S_ISDIR(status->st_mode))
    {
        written = sprintf(buffer, "nume director: %s\nidentificatorul utilizatorului: %d"
                                  "\ndrepturi de acces user: %c%c%c\n"
                                  "drepturi de acces grup: %c%c%c\ndrepturi de acces altii: %c%c%c\n\n\n",
                          filename,
                          status->st_uid,
                          (status->st_mode & S_IRUSR) ? 'R' : '-',
                          (status->st_mode & S_IWUSR) ? 'W' : '-',
                          (status->st_mode & S_IXUSR) ? 'X' : '-',
                          (status->st_mode & S_IRGRP) ? 'R' : '-',
                          (status->st_mode & S_IWGRP) ? 'W' : '-',
                          (status->st_mode & S_IXGRP) ? 'X' : '-',
                          (status->st_mode & S_IROTH) ? 'R' : '-',
                          (status->st_mode & S_IWOTH) ? 'W' : '-',
                          (status->st_mode & S_IXOTH) ? 'X' : '-');
        if (written > 0)
        {
            write(fd_statistica, buffer, written);
        }
    }
}

int count_lines(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Eroare la deschiderea fisierului!!");
        exit(EXIT_FAILURE);
    }

    int line_count = 0;
    ssize_t bytes_read;
    char buffer[1024];

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            if (buffer[i] == '\n')
            {
                line_count++;
            }
        }
    }

    if (bytes_read == -1)
    {
        perror("Eroare la citirea fisierului");
        exit(EXIT_FAILURE);
    }

    close(fd);
    return line_count;
}

void write_statistica_fisiere(const char *input_path, char *output_dir, struct stat *status)
{
    char *input_path_copy1, *input_path_copy2;
    char output_path[257];
    char *dir_name, *filename;

    input_path_copy1 = strdup(input_path);
    input_path_copy2 = strdup(input_path);

    dir_name = dirname(input_path_copy1);
    filename = basename(input_path_copy2);

    snprintf(output_path, sizeof(output_path), "%s/%s_statistica.txt", output_dir, filename);

    int fd_statistica = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_statistica == -1)
    {
        perror("Eroare la deschiderea fisierului de statistica");
        exit(EXIT_FAILURE);
    }

    write_statistica(filename, dir_name, output_dir, status, fd_statistica);

    close(fd_statistica);
    free(input_path_copy1);
    free(input_path_copy2);
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        perror("Nr gresit de argumente");
        exit(EXIT_FAILURE);
    }

    int fis_statistica = open("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fis_statistica == -1)
    {
        perror("Eroare la deschiderea statistica.txt");
        exit(EXIT_FAILURE);
    }

    DIR *dir = opendir(argv[1]);
    if (!dir)
    {
        perror("Eroare la deschiderea directorului.");
        close(fis_statistica);
        exit(EXIT_FAILURE);
    }

    struct dirent *dir_ent;
    struct stat status;

    int written_stat;
    char buffer[1024];

    while ((dir_ent = readdir(dir)) != NULL)
    {
        if (strcmp(dir_ent->d_name, ".") == 0 || strcmp(dir_ent->d_name, "..") == 0) {
            continue;
        }
        char full_path[257];      // director_intare/fisier.txt
        char full_path_stat[300]; // director_iesire/fisier_statistica.txt

        snprintf(full_path, sizeof(full_path), "%s/%s", argv[1], dir_ent->d_name);
        snprintf(full_path_stat, sizeof(full_path_stat), "%s/%s_statistica.txt", argv[2], dir_ent->d_name);
        if (lstat(full_path, &status) == 0)
        {
            pid_t pid = fork();
            if (pid == 0)
            { // Procesul copil
                write_statistica_fisiere(full_path, argv[2], &status);
                // convert(full_path);
                written_stat = sprintf(buffer, "Nr de linii scrise pentru %s_statistica.txt este %d\n", dir_ent->d_name, count_lines(full_path_stat));
                write(fis_statistica, buffer, written_stat);
                exit(0); // Terminarea procesului copil
            }
            else if (pid > 0)
            { // Procesul părinte
                int status;
                wait(&status);
                if (WIFEXITED(status))
                {
                    printf("S-a încheiat procesul cu pid-ul %d și codul %d\n", pid, WEXITSTATUS(status));
                }
            }
            else
            {
                perror("Eroare la fork");
                exit(EXIT_FAILURE);
            }

            if (is_bmp(dir_ent->d_name, argv[1]))
            {
                pid_t pid2 = fork();
                if (pid2 == 0)
                { // Procesul copil
                    convert(full_path);
                    exit(0); // Terminarea procesului copil
                }
                else if (pid2 > 0)
                { // Procesul părinte
                    int status;
                    wait(&status);
                    if (WIFEXITED(status))
                    {
                        printf("S-a încheiat procesul cu pid-ul %d și codul %d\n", pid, WEXITSTATUS(status));
                    }
                }
                else
                {
                    perror("Eroare la fork");
                    exit(EXIT_FAILURE);
                }
            }
        }
        else
        {
            perror("Eroare la statusul fișierului");
        }
    }

    closedir(dir);
    close(fis_statistica);
    return 0;
}
