
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 4096
#define MAX_FNAME_LEN 4096

static uint64_t htonll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((uint64_t)htonl((uint32_t)(v & 0xffffffff)) << 32) | htonl((uint32_t)(v >> 32));
#else
    return v;
#endif
}
static uint64_t ntohll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((uint64_t)ntohl((uint32_t)(v & 0xffffffff)) << 32) | ntohl((uint32_t)(v >> 32));
#else
    return v;
#endif
}

ssize_t send_all(int fd, const void *buf, size_t n) {
    size_t total = 0;
    const char *p = buf;
    while (total < n) {
        ssize_t s = send(fd, p + total, n - total, 0);
        if (s < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += s;
    }
    return total;
}

ssize_t recv_all(int fd, void *buf, size_t n) {
    size_t total = 0;
    char *p = buf;
    while (total < n) {
        ssize_t r = recv(fd, p + total, n - total, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return total;
        total += r;
    }
    return total;
}

int main(int argc, char **argv) {
    const char *host = NULL;
    int port = 9000;
    const char *filepath = NULL;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--host") == 0 || strcmp(argv[i], "-h") == 0) && i + 1 < argc) { host = argv[++i]; }
        else if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) { port = atoi(argv[++i]); }
        else if ((strcmp(argv[i], "--file") == 0 || strcmp(argv[i], "-f") == 0) && i + 1 < argc) { filepath = argv[++i]; }
        else { fprintf(stderr, "Unknown arg: %s\n", argv[i]); }
    }

    if (!host || !filepath) {
        fprintf(stderr, "Usage: %s --host SERVER_IP --port PORT --file path/to/file\n", argv[0]);
        return 1;
    }

    // stat file
    struct stat st;
    if (stat(filepath, &st) < 0) { perror("stat"); return 1; }
    if (!S_ISREG(st.st_mode)) { fprintf(stderr, "Not a regular file: %s\n", filepath); return 1; }
    uint64_t filesize = (uint64_t)st.st_size;

    // get basename
    const char *base = strrchr(filepath, '/');
    base = base ? base + 1 : filepath;
    size_t fname_len = strlen(base);
    if (fname_len == 0 || fname_len > MAX_FNAME_LEN) { fprintf(stderr, "Invalid filename length\n"); return 1; }

    // connect
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) { fprintf(stderr, "Invalid host: %s\n", host); close(sock); return 1; }

    printf("[+] Connecting to %s:%d ...\n", host, port);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("connect"); close(sock); return 1; }

    // prepare header: 4 bytes fname_len (network), fname bytes, 8 bytes filesize (network)
    uint32_t fname_len_net = htonl((uint32_t)fname_len);
    uint64_t filesize_net = htonll(filesize);

    if (send_all(sock, &fname_len_net, sizeof(fname_len_net)) != sizeof(fname_len_net)) { perror("send"); close(sock); return 1; }
    if (send_all(sock, base, fname_len) != (ssize_t)fname_len) { perror("send"); close(sock); return 1; }
    if (send_all(sock, &filesize_net, sizeof(filesize_net)) != sizeof(filesize_net)) { perror("send"); close(sock); return 1; }

    // send file bytes
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) { perror("open"); close(sock); return 1; }

    uint64_t sent = 0;
    char buf[BUF_SIZE];
    while (sent < filesize) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("read");
            close(fd);
            close(sock);
            return 1;
        }
        if (r == 0) break;
        if (send_all(sock, buf, r) != r) {
            perror("send");
            close(fd);
            close(sock);
            return 1;
        }
        sent += (uint64_t)r;
        // simple progress
        fprintf(stderr, "\r[>] Sent %" PRIu64 "/%" PRIu64 " bytes", sent, filesize);
    }
    fprintf(stderr, "\n");
    close(fd);

    // receive server response (up to 128 bytes)
    char resp[128] = {0};
    ssize_t rr = recv(sock, resp, sizeof(resp)-1, 0);
    if (rr > 0) {
        printf("[+] Server response: %.*s", (int)rr, resp);
        if (resp[rr-1] != '\n') printf("\n");
    } else {
        printf("[!] No response from server\n");
    }

    close(sock);
    return 0;
}
