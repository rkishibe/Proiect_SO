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

#define BMP_SIZE_OFFSET 2
#define BMP_WIDTH_OFFSET 18
#define BMP_HEIGHT_OFFSET 22
#define BMP_BITCOUNT_OFFSET 28
#define BMP_IMAGE_SIZE_OFFSET 34
#define BMP_HEADER_SIZE 54
#define getName(var) #var

typedef struct RGB
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB; //struct to store a pixel's data

typedef struct BMPHeader
{
    uint32_t size;
    int32_t width_px;
    int32_t height_px;
    uint32_t image_size_bytes;
    uint16_t bitCount;
    RGB rgb;
} BMPHeader; //struct to store required metadata

/* utility functions for common operations with files and pipes */
int open_file(char *filename, int flag)
{
    int fd_stat;
    if ((fd_stat = open(filename, flag)) == -1)
    {
        perror("error in opening file");
        exit(EXIT_FAILURE);
    }
    return fd_stat;
}

void close_file(int fd)
{
    if (close(fd) == -1)
    {
        perror("error in closing file");
        exit(EXIT_FAILURE);
    }
}

void close_pipe_read(int pipe)
{
    if (close(pipe) < 0) // inchid capat de citire de la frate, eu am nevoie doar de cel de scriere
    {
        printf("Error closing reading pipe end %s", getName(pipe));
        exit(EXIT_FAILURE);
    }
}

void close_pipe_write(int pipe)
{
    if (close(pipe) < 0) // inchid capat de citire de la frate, eu am nevoie doar de cel de scriere
    {
        printf("Error closing writing pipe end %s", getName(pipe));
        exit(EXIT_FAILURE);
    }
}
/*end of utility functions*/

/*bmp processing functions*/
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
    bmp_header->bitCount = *((uint16_t *)&buffer[BMP_BITCOUNT_OFFSET - BMP_SIZE_OFFSET]);
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

void convert(char *full_path)
{
    int bmp_file;
    RGB *pixels = NULL;
    BMPHeader bmp_header;

    bmp_file = open_file(full_path, O_RDWR);

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

    bmp_file = open_file(full_path, O_RDONLY);

    read_bmp_header(bmp_file, &bmp_header);
    fstat(bmp_file, &bmp_stat);
    close_file(bmp_file);

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
/*end of bmp processing functions*/

/*statistics functions*/
int count_lines(char *filename)
{
    int fd = open_file(filename, O_RDONLY);

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

    close_file(fd);
    return line_count;
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

    int fd_statistica = open_file(output_path, O_WRONLY | O_CREAT | O_TRUNC);

    write_statistica(filename, dir_name, output_dir, status, fd_statistica);

    close_file(fd_statistica);
    free(input_path_copy1);
    free(input_path_copy2);
}

void count_lines_proc(char *full_path, struct stat status, int fis_statistica, struct dirent *dir_ent, char *full_path_stat, char *output_dir)
{
    int written_stat;
    char buffer[1024];
    pid_t pid = fork();
    if (pid == 0)
    { // Procesul copil

        write_statistica_fisiere(full_path, output_dir, &status);
        written_stat = sprintf(buffer, "Nr de linii scrise pentru %s_statistica.txt este %d\n", dir_ent->d_name, count_lines(full_path_stat));
        write(fis_statistica, buffer, written_stat);
        exit(0); // Terminarea procesului copil
    }
}
/*end of statistics functions*/

/*count of charcter occurences using the shell script*/
int count_sentences(int FF[2], int PF[2], char *c)
{
    pid_t pid3 = fork(); 
    if (pid3 == 0)
    {
        close_pipe_write(FF[1]);

        close_pipe_read(PF[0]);
        dup2(FF[0], 0);
        dup2(PF[1], 1);

        close_pipe_write(PF[1]);
        close_pipe_read(FF[0]);

        execlp("bash", "bash", "caracter.sh", c, (char *)NULL);
        perror("Failed to execute caracter.sh");
        exit(EXIT_FAILURE);
    }
    else if (pid3 < 0)
    {
        perror("Eroare la fork");
        exit(EXIT_FAILURE);
    }
    return pid3;
}

int main(int argc, char *argv[])
{
    int PF[2]; // capete de scrie si citire pt parinte-fiu
    int FF[2]; // capete de scriere si citire pt fiu-fiu
    struct dirent *dir_ent;
    struct stat status;

    int written_stat;
    char buffer[1024];

    int total_propoz = 0;

    if (argc != 4)
    {
        perror("Nr gresit de argumente");
        exit(EXIT_FAILURE);
    }

    char c = argv[3][0];

    int fis_statistica = open_file("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC);

    DIR *dir = opendir(argv[1]);
    if (!dir)
    {
        perror("Eroare la deschiderea directorului.");
        close_file(fis_statistica);
        exit(EXIT_FAILURE);
    }

    while ((dir_ent = readdir(dir)) != NULL)
    {
        if (strcmp(dir_ent->d_name, ".") == 0 || strcmp(dir_ent->d_name, "..") == 0)
        {
            continue;
        }
        char full_path[257];      // director_intare/fisier.txt
        char full_path_stat[300]; // director_iesire/fisier_statistica.txt

        snprintf(full_path, sizeof(full_path), "%s/%s", argv[1], dir_ent->d_name);
        snprintf(full_path_stat, sizeof(full_path_stat), "%s/%s_statistica.txt", argv[2], dir_ent->d_name);
        if (lstat(full_path, &status) == 0)
        {
            if (S_ISREG(status.st_mode))
            {
                if (is_bmp(dir_ent->d_name, argv[1])) //! e bmp
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
                            printf("S-a încheiat procesul cu pid-ul %d și codul %d\n", pid2, WEXITSTATUS(status));
                        }
                    }
                    else
                    {
                        perror("Eroare la fork");
                        exit(EXIT_FAILURE);
                    }

                    count_lines_proc(full_path, status, fis_statistica, dir_ent, full_path_stat, argv[2]);
                } //! gata bmp
                else
                { //! fis normal
                    if (pipe(PF) < 0 || pipe(FF) < 0)
                    {
                        perror("Failed to create pipes");
                        exit(EXIT_FAILURE);
                    }
                    pid_t pid = fork();
                    if (pid == 0)
                    { // Procesul copil
                        close_pipe_read(FF[0]);
                        close_pipe_read(PF[0]);
                        close_pipe_write(PF[1]);

                        write_statistica_fisiere(full_path, argv[2], &status);
                        written_stat = sprintf(buffer, "Nr de linii scrise pentru %s_statistica.txt este %d\n", dir_ent->d_name, count_lines(full_path_stat));
                        write(fis_statistica, buffer, written_stat);

                        dup2(FF[1], 1);
                        close_pipe_write(FF[1]);

                        char comanda[1024];
                        sprintf(comanda, "cat %s ; exit %d", full_path, count_lines(full_path_stat));

                        execlp("bash", "bash", "-c", comanda, (char *)NULL);
                        perror("Failed to execute cat");
                        exit(EXIT_FAILURE);
                    }
                    pid_t pid3=count_sentences(FF, PF ,argv[3]); //! count propoz

                    close_pipe_read(FF[0]);
                    close_pipe_write(FF[1]);
                    close_pipe_write(PF[1]);

                    int status;
                    waitpid(pid, &status, 0);
                    waitpid(pid3, NULL, 0);

                    if (WIFEXITED(status))
                    {
                        printf("S-a încheiat procesul cu pid-ul %d și codul %d\n", pid3, WEXITSTATUS(status));
                    }
                    char buffer[512];
                    if (read(PF[0], buffer, sizeof(buffer)) < 0)
                    {
                        perror("err la read");
                        exit(EXIT_FAILURE);
                    }
                    close_pipe_read(PF[0]);

                    int n = atoi(buffer);
                    total_propoz += n;

                } //! gata fis normal
            }
            if (S_ISDIR(status.st_mode))
            {
                count_lines_proc(full_path, status, fis_statistica, dir_ent, full_path_stat, argv[2]);
            }
            if (S_ISLNK(status.st_mode))
            {
                count_lines_proc(full_path, status, fis_statistica, dir_ent, full_path_stat, argv[2]);
            }
        }
        else
        {
            perror("Eroare la statusul fișierului");
        }
    }
    int status2;
    int pidfiu;
    while ((pidfiu = wait(&status2)) > 0)
    {
        if (WIFEXITED(status2))
        {
            printf("S-a încheiat procesul cu pid-ul %d și codul %d\n", pidfiu, WEXITSTATUS(status2));
        }
    }
    printf("Au fost identificate in total %d propozitii corecte care contin caracterul %c\n", total_propoz, c);
    closedir(dir);
    close_file(fis_statistica);
    return 0;
}
