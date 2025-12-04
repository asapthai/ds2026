#include "file_transfer.h"
#include <stdio.h>
#include <stdlib.h>
/*START_TRANSFER Opens the file in "write" mode to clear,create , then closes*/
int *
start_transfer_1_svc(char **filename, struct svc_req *rqstp)
{
    static int  result;
    FILE *f;
    printf("[Server] Request to start transfer for: %s\n", *filename);
    /*open with "w" to clear existing file or create new one*/
    f = fopen(*filename, "w");
    if (f == NULL) {
        perror("fopen");
        result = 1; /*if error */
        return &result;
    }
    fclose(f);
    result = 0; /*if success */
    return &result;
}
/*WRITE_CHUNK Opens the file in "append" mode, adds data, and closes*/
int *
write_chunk_1_svc(struct file_chunk *argp, struct svc_req *rqstp)
{
    static int  result;
    FILE *f;
    /*open with "ab" (append binary)*/
    f = fopen(argp->filename, "ab");
    if (f == NULL) {
        perror("fopen");
        result = -1;
        return &result;
    }
    /*write the specific bytes from the chunk*/
    size_t written = fwrite(argp->data.data_val, 1, argp->data.data_len, f);
    fclose(f);
    if (written != argp->data.data_len) {
        printf("[Server] Error writing chunk\n");
        result = -1;
    } else {
        result = (int)written;
    }
    return &result;
}