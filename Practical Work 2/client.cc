#include "file_transfer.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#define CHUNK_SIZE 4096

void
file_transfer_prog_1(char *host, char *filepath)
{
    CLIENT *clnt;
    int  *result_1;
    char *start_transfer_1_arg;
    int  *result_2;
    struct file_chunk  write_chunk_1_arg;
    FILE *f;
    char buffer[CHUNK_SIZE];
    size_t bytes_read;
    unsigned long total_sent = 0;
    /*create client handle (using TCP for reliable transfer)*/
    clnt = clnt_create(host, FILE_TRANSFER_PROG, FILE_TRANSFER_VERS, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(host);
        exit(1);
    }
    /*prepare filename*/
    char *base = strrchr(filepath, '/');
    base = base ? base + 1 : filepath;
    start_transfer_1_arg = base;
    /*call START_TRANSFER to initialize file on server*/
    printf("[Client] Initializing transfer for '%s'...\n", base);
    result_1 = start_transfer_1( &start_transfer_1_arg, clnt);
    if (result_1 == (int *) NULL) {
        clnt_perror(clnt, "call failed");
        exit(1);
    }
    if (*result_1 != 0) {
        fprintf(stderr, "[Client] Server failed to create file.\n");
        exit(1);
    }
    /*open local file and loop through chunks*/
    f = fopen(filepath, "rb");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    /*setup constant part of arguments*/
    write_chunk_1_arg.filename = base;

    printf("[Client] Sending data...\n");
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, f)) > 0) {
        /*set up the dynamic data for RPC*/
        write_chunk_1_arg.data.data_len = bytes_read;
        write_chunk_1_arg.data.data_val = buffer;
        write_chunk_1_arg.bytes_len = bytes_read;
        /*call WRITE_CHUNK*/
        result_2 = write_chunk_1(&write_chunk_1_arg, clnt);
        if (result_2 == (int *) NULL) {
            clnt_perror(clnt, "transfer call failed");
            fclose(f);
            exit(1);
        }
        if (*result_2 < 0) {
            fprintf(stderr, "Server reported write error.\n");
            break;
        }
        total_sent += *result_2;
        printf("\r[Client] Sent: %lu bytes", total_sent);
    }
    printf("\n[Client] Transfer complete.\n");
    fclose(f);
    clnt_destroy(clnt);
}
int
main(int argc, char *argv[])
{
    char *host;
    char *file;
    if (argc < 3) {
        printf("usage: %s server_host filename\n", argv[0]);
        exit(1);
    }
    host = argv[1];
    file = argv[2];
    file_transfer_prog_1(host, file);
    exit(0);
}