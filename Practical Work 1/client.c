#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#define BUF 4096
static uint64_t htonll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((uint64_t)htonl(v & 0xffffffff) << 32) | htonl(v >> 32);
#else
    return v;
#endif
}
static ssize_t send_all(int fd, const void *b, size_t n) {
    size_t t = 0;
    while (t < n) {
        ssize_t s = send(fd, (char*)b + t, n - t, 0);
        if (s <= 0) return -1;
        t += s;
    }
    return t;
}
int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "Usage: %s --host IP --port P --file PATH\n", argv[0]);
        return 1;
    }
    const char *host = NULL, *file = NULL;
    int port = 0;
    // simple arg parsing
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--host")) host = argv[++i];
        else if (!strcmp(argv[i], "--port")) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--file")) file = argv[++i];
    }
    struct stat st;
    if (stat(file, &st) < 0 || !S_ISREG(st.st_mode)) {
        perror("stat");
        return 1;
    }
    uint64_t fsize = st.st_size;
    const char *name = strrchr(file, '/');
    name = name ? name + 1 : file;
    uint32_t namelen = strlen(name);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    inet_pton(AF_INET, host, &a.sin_addr);
    connect(s, (struct sockaddr*)&a, sizeof(a));
    // send header
    uint32_t nl = htonl(namelen);
    uint64_t sz = htonll(fsize);
    send_all(s, &nl, 4);
    send_all(s, name, namelen);
    send_all(s, &sz, 8);
    // send file
    int fd = open(file, O_RDONLY);
    char buf[BUF];
    ssize_t r;
    while ((r = read(fd, buf, BUF)) > 0)
        send_all(s, buf, r);
    close(fd);
    // read server response
    char resp[64] = {0};
    recv(s, resp, sizeof(resp)-1, 0);
    printf("[Server] %s\n", resp);

    close(s);
    return 0;
}
