#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 4096
#define TAG_FILENAME 1
#define TAG_CONTENT 2
#define TAG_END 3
//reads file and sends it
void run_sender(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        perror("[Sender] Error opening file");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    //send Filename (Metadata)
    const char *basename = strrchr(filepath, '/');
    basename = basename ? basename + 1 : filepath;
    int name_len = strlen(basename) + 1;
    MPI_Send(basename, name_len, MPI_CHAR, 1, TAG_FILENAME, MPI_COMM_WORLD);
    printf("[Sender] Transferring '%s'...\n", basename);
    // 2. Send Content (Data Chunks)
    char buffer[CHUNK_SIZE];
    size_t bytes_read;
    unsigned long total_sent = 0;
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, f)) > 0) {
        MPI_Send(buffer, bytes_read, MPI_BYTE, 1, TAG_CONTENT, MPI_COMM_WORLD);
        total_sent += bytes_read;
    }
    //send end signal
    MPI_Send(buffer, 0, MPI_BYTE, 1, TAG_END, MPI_COMM_WORLD);
    fclose(f);
    printf("[Sender] Done. Sent %lu bytes.\n", total_sent);
}
// receives data and writes to file
void run_receiver() {
    MPI_Status status;
    char filename[1024];
    char buffer[CHUNK_SIZE];
    int bytes_received;

    // receive Filename
    MPI_Recv(filename, 1024, MPI_CHAR, 0, TAG_FILENAME, MPI_COMM_WORLD, &status);
    
    char outpath[1100];
    snprintf(outpath, sizeof(outpath), "received_%s", filename);
    printf("[Receiver] Saving to '%s'...\n", outpath);

    FILE *f = fopen(outpath, "wb");
    if (!f) {
        perror("[Receiver] Error creating file");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // receive Loop
    while (1) {
        MPI_Recv(buffer, CHUNK_SIZE, MPI_BYTE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TAG_END) {
            break;
        }
        MPI_Get_count(&status, MPI_BYTE, &bytes_received);
        fwrite(buffer, 1, bytes_received, f);
    }
    fclose(f);
    printf("[Receiver] File saved successfully.\n");
}
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    // routing based on Rank
    if (world_rank == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: mpiexec -n 2 %s <filename>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        run_sender(argv[1]);
    } else if (world_rank == 1) {
        run_receiver();
    }
    MPI_Finalize();
    return 0;
}