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
#define MAXN 4096
static uint64_t ntohll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((uint64_t)ntohl(v & 0xffffffff) << 32) | ntohl(v >> 32);
#else
    return v;
#endif
}
static ssize_t recv_all(int fd, void *b, size_t n) {
    size_t t = 0;
    while (t < n) {
        ssize_t r = recv(fd, (char*)b + t, n - t, 0);
        if (r <= 0) return r == 0 ? t : -1;
        t += r;
    }
    return t;
}
int main(int argc, char **argv) {
    int port = 9000;
    const char *outdir = "received_files";
    mkdir(outdir, 0755);
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = INADDR_ANY;
    bind(ls, (struct sockaddr*)&a, sizeof(a));
    listen(ls, 5);
    printf("Listening on 0.0.0.0:%d\n", port);
    while (1) {
        int c = accept(ls, NULL, NULL);
        uint32_t namelen;
        uint64_t fsize;
        char name[MAXN+1];
        char buf[BUF];
        // recv header
        recv_all(c, &namelen, 4);
        namelen = ntohl(namelen);
        recv_all(c, name, namelen);
        name[namelen] = 0;
        recv_all(c, &fsize, 8);
        fsize = ntohll(fsize);
        // build file path
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", outdir, name);
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        uint64_t recvd = 0;
        ssize_t r;
        while (recvd < fsize && (r = recv(c, buf, BUF, 0)) > 0) {
            write(fd, buf, r);
            recvd += r;
        }
        close(fd);
        if (recvd == fsize) send(c, "OK\n", 3, 0);
        else send(c, "INCOMPLETE\n", 11, 0);
        close(c);
    }
    close(ls);
    return 0;
}
