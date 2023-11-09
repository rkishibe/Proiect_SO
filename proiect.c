#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>

typedef struct BMPHeader {
    uint32_t size;
    int32_t  width_px;
    int32_t  height_px;
    uint32_t image_size_bytes;
} BMPHeader;

#define BMP_SIZE_OFFSET 2
#define BMP_WIDTH_OFFSET 18
#define BMP_HEIGHT_OFFSET 22
#define BMP_IMAGE_SIZE_OFFSET 34

int read_bmp_header(int fd, BMPHeader *bmp_header) {
    char buffer[54];
    if (lseek(fd, BMP_SIZE_OFFSET, SEEK_SET) == -1) {
        perror("Eroare la lseek");
        return -1;
    }
    if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
        perror("Eroare la citirea header-ului BMP");
        return -1;
    }

    bmp_header->size = *((uint32_t*)&buffer[BMP_SIZE_OFFSET - BMP_SIZE_OFFSET]);
    bmp_header->width_px = *((int32_t*)&buffer[BMP_WIDTH_OFFSET - BMP_SIZE_OFFSET]);
    bmp_header->height_px = *((int32_t*)&buffer[BMP_HEIGHT_OFFSET - BMP_SIZE_OFFSET]);
    bmp_header->image_size_bytes = *((uint32_t*)&buffer[BMP_IMAGE_SIZE_OFFSET - BMP_SIZE_OFFSET]);

    return 0;
}

void write_bmp_info(const char *filename, int fd_statistica) {
    BMPHeader bmp_header;
    struct stat bmp_stat;
    struct tm *mod_time;
    char buffer[1024];
    int written, bmp_file;

    bmp_file = open(filename, O_RDONLY);
    if (bmp_file == -1) {
        perror("Eroare la deschiderea fisierului BMP");
        exit(-1);
    }

    if (read_bmp_header(bmp_file, &bmp_header) != 0) {
        perror("Eroare la citirea fisierului BMP");
        exit(-1);
    }

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
        (bmp_stat.st_mode & S_IWGRP) ? '-' : '-',
        (bmp_stat.st_mode & S_IXGRP) ? '-' : '-',
        (bmp_stat.st_mode & S_IROTH) ? '-' : '-',
        (bmp_stat.st_mode & S_IWOTH) ? '-' : '-',
        (bmp_stat.st_mode & S_IXOTH) ? '-' : '-'
    );

    if (written > 0) {
        write(fd_statistica, buffer, written);
    }
}

void write_statistica(const char *filename, struct stat *status, int fd_statistica) {
    char buffer[1024];
    int written;
    struct tm *mod_time;

    if (S_ISLNK(status->st_mode)) {
        struct stat target_status;
        char target_path[257];
        ssize_t len = readlink(filename, target_path, sizeof(target_path) - 1);
        if (len != -1) {
            target_path[len] = '\0';
            if (stat(target_path, &target_status) == -1) {
                perror("Eroare la status");
                return;
            }

            written = sprintf(buffer, "nume legatura: %s\ndimensiune legatura: %ld\n"
                                      "dimensiune fisier: %ld\ndrepturi de acces user legatura: %c%c%c\n",
                                      filename, status->st_size, target_status.st_size,
                                      (status->st_mode & S_IRUSR) ? 'R' : '-',
                                      (status->st_mode & S_IWUSR) ? 'W' : '-',
                                      (status->st_mode & S_IXUSR) ? 'X' : '-');
            if (written > 0) {
                write(fd_statistica, buffer, written);
            }
        }
    } else if (S_ISREG(status->st_mode)) {
        mod_time = localtime(&status->st_mtime);
        char time_str[11];
        strftime(time_str, sizeof(time_str), "%d.%m.%Y", mod_time);
        const char *ext = strrchr(filename, '.');
        if (ext && strcmp(ext, ".bmp") == 0) {
            write_bmp_info(filename,fd_statistica);
        } else {
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
        (status->st_mode & S_IWGRP) ? '-' : '-',
        (status->st_mode & S_IXGRP) ? '-' : '-',
        (status->st_mode & S_IROTH) ? '-' : '-',
        (status->st_mode & S_IWOTH) ? '-' : '-',
        (status->st_mode & S_IXOTH) ? '-' : '-'
    );
            if (written > 0) {
                write(fd_statistica, buffer, written);
            }
        }
    } else if (S_ISDIR(status->st_mode)) {
        written = sprintf(buffer, "nume director: %s\nidentificatorul utilizatorului: %d"
        "\ndrepturi de acces user: %c%c%c\n"
        "drepturi de acces grup: %c%c%c\ndrepturi de acces altii: %c%c%c\n\n\n", 
        filename, 
        status->st_uid,
        (status->st_mode & S_IRUSR) ? 'R' : '-',
        (status->st_mode & S_IWUSR) ? 'W' : '-',
        (status->st_mode & S_IXUSR) ? 'X' : '-',
        (status->st_mode & S_IRGRP) ? 'R' : '-',
        (status->st_mode & S_IWGRP) ? '-' : '-',
        (status->st_mode & S_IXGRP) ? '-' : '-',
        (status->st_mode & S_IROTH) ? '-' : '-',
        (status->st_mode & S_IWOTH) ? '-' : '-',
        (status->st_mode & S_IXOTH) ? '-' : '-');

        if (written > 0) {
            write(fd_statistica, buffer, written);
        }
    }
}

int main(int argc, char *argv[]) {
    int fis_statistica = open("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fis_statistica == -1) {
        perror("Eroare la deschiderea statistica.txt");
        exit(EXIT_FAILURE);
    }

    DIR *dir = opendir(argv[1]);
    if (!dir) {
        perror("Eroare la deschiderea directorului.");
        close(fis_statistica);
        exit(-1);
    }

    struct dirent *dir_ent;
    struct stat status;

    while ((dir_ent = readdir(dir)) != NULL) {
        char full_path[257];
        snprintf(full_path, sizeof(full_path), "%s/%s", argv[1], dir_ent->d_name);
        if (lstat(full_path, &status) == 0) {
            write_statistica(full_path, &status, fis_statistica);
        } else {
            perror("Eroare la statusul fisierului.");
        }
    }

    close(fis_statistica);
    closedir(dir);
    return 0;
}
