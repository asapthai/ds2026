
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

#ifndef PORT_DEFAULT
#define PORT_DEFAULT 9000
#endif

#define BACKLOG 5
#define BUF_SIZE 4096
#define MAX_FNAME_LEN 4096

// htonll / ntohll helpers
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

// read exactly n bytes or return -1
ssize_t recv_all(int fd, void *buf, size_t n) {
    size_t total = 0;
    char *p = buf;
    while (total < n) {
        ssize_t r = recv(fd, p + total, n - total, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return total; // EOF
        total += r;
    }
    return total;
}

// send exactly n bytes or return -1
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

int handle_connection(int client_fd, const char *outdir) {
    // receive 4 bytes filename length
    uint32_t fname_len_net;
    ssize_t r = recv_all(client_fd, &fname_len_net, sizeof(fname_len_net));
    if (r != sizeof(fname_len_net)) {
        fprintf(stderr, "[-] Failed to read filename length\n");
        return -1;
    }
    uint32_t fname_len = ntohl(fname_len_net);
    if (fname_len == 0 || fname_len > MAX_FNAME_LEN) {
        fprintf(stderr, "[-] Filename length invalid: %u\n", fname_len);
        return -1;
    }

    char *fname = malloc(fname_len + 1);
    if (!fname) { perror("malloc"); return -1; }

    r = recv_all(client_fd, fname, fname_len);
    if (r != (ssize_t)fname_len) {
        fprintf(stderr, "[-] Failed to read filename bytes\n");
        free(fname);
        return -1;
    }
    fname[fname_len] = '\0';

    // receive 8 bytes filesize
    uint64_t fsize_net;
    r = recv_all(client_fd, &fsize_net, sizeof(fsize_net));
    if (r != sizeof(fsize_net)) {
        fprintf(stderr, "[-] Failed to read filesize\n");
        free(fname);
        return -1;
    }
    uint64_t filesize = ntohll(fsize_net);

    printf("[+] Receiving '%s' (%" PRIu64 " bytes)\n", fname, filesize);

    // ensure outdir exists
    if (mkdir(outdir, 0755) < 0 && errno != EEXIST) {
        perror("mkdir");
        free(fname);
        return -1;
    }

    // prepare output path (outdir / basename(fname))
    const char *base = strrchr(fname, '/');
    base = base ? base + 1 : fname;
    char outpath[PATH_MAX];
    snprintf(outpath, sizeof(outpath), "%s/%s", outdir, base);

    int outfd = open(outpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (outfd < 0) {
        perror("open");
        free(fname);
        return -1;
    }

    // receive file bytes
    uint64_t received = 0;
    char buf[BUF_SIZE];
    while (received < filesize) {
        size_t toread = (size_t)((filesize - received) > BUF_SIZE ? BUF_SIZE : (filesize - received));
        ssize_t got = recv(client_fd, buf, toread, 0);
        if (got < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            close(outfd);
            free(fname);
            return -1;
        }
        if (got == 0) break; // peer closed
        ssize_t wrote = write(outfd, buf, got);
        if (wrote < 0) {
            perror("write");
            close(outfd);
            free(fname);
            return -1;
        }
        received += (uint64_t)got;
    }

    close(outfd);

    if (received == filesize) {
        printf("[+] Transfer complete: %s (%" PRIu64 " bytes)\n", outpath, received);
        send_all(client_fd, "OK\n", 3);
    } else {
        fprintf(stderr, "[-] Transfer incomplete: expected %" PRIu64 ", got %" PRIu64 "\n", filesize, received);
        send_all(client_fd, "INCOMPLETE\n", 11);
    }

    free(fname);
    return 0;
}

int main(int argc, char **argv) {
    const char *host = "0.0.0.0";
    int port = PORT_DEFAULT;
    const char *outdir = "received_files";

    // simple arg parsing
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--host") == 0 || strcmp(argv[i], "-h") == 0) && i + 1 < argc) { host = argv[++i]; }
        else if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) { port = atoi(argv[++i]); }
        else if ((strcmp(argv[i], "--outdir") == 0 || strcmp(argv[i], "-o") == 0) && i + 1 < argc) { outdir = argv[++i]; }
        else { fprintf(stderr, "Unknown arg: %s\n", argv[i]); }
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        // allow host "0.0.0.0"
        if (strcmp(host, "0.0.0.0") == 0) addr.sin_addr.s_addr = INADDR_ANY;
        else { fprintf(stderr, "Invalid host: %s\n", host); close(listen_fd); return 1; }
    }

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(listen_fd); return 1; }
    if (listen(listen_fd, BACKLOG) < 0) { perror("listen"); close(listen_fd); return 1; }

    printf("[+] Listening on %s:%d, saving files to '%s'\n", host, port, outdir);

    while (1) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int client_fd = accept(listen_fd, (struct sockaddr *)&peer, &plen);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
        printf("[+] Connection from %s:%d\n", peer_ip, ntohs(peer.sin_port));

        // handle single connection (blocking). For multi-connections, fork/threads would be needed.
        handle_connection(client_fd, outdir);

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}
