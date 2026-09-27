#include "common.h"
#include "catalog.h"
#include "network.h"
#include "protocol.h"
#include "transfer.h"
#include "peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static volatile int g_server_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    g_server_running = 0;
    /* Print a newline so ^C does not look messy */
    printf("\nShutting down server...\n");
    fflush(stdout);
}

/**
 * Validates and converts a port string into an integer [1, 65535].
 * Returns port on success, or -1 on invalid syntax / out-of-range value.
 */
static int parse_port(const char *str) {
    if (!str || *str == '\0') {
        return -1;
    }
    char *endptr = NULL;
    long val = strtol(str, &endptr, 10);
    if (*endptr != '\0' || val <= 0 || val > 65535) {
        return -1;
    }
    return (int)val;
}

static void print_usage(const char *prog_name) {
    printf("P2P File Transfer System (Mid-Semester Version)\n");
    printf("Usage:\n");
    printf("  %s server <port> <shared_directory>\n", prog_name);
    printf("  %s get <server_ip> <server_port> <filename> <output_path>\n", prog_name);
    printf("  %s list <shared_directory>\n", prog_name);
    printf("\nExamples:\n");
    printf("  %s server 5000 ./shared\n", prog_name);
    printf("  %s get 127.0.0.1 5000 report.pdf ./downloads/report.pdf\n", prog_name);
    printf("  %s list ./shared\n", prog_name);
}

/**
 * Runs the peer in server (provider) mode.
 * Listens for requests sequentially and serves requested files from catalog.
 */
static int run_server(int port, const char *shared_dir) {
    if (network_init() != 0) {
        log_error("Network initialization failed");
        return 1;
    }

    FileCatalog catalog;
    catalog_init(&catalog);

    /* Scan directory and build linked list catalog */
    if (catalog_scan_directory(&catalog, shared_dir) != 0) {
        log_error("Could not scan shared directory '%s'", shared_dir);
        catalog_free(&catalog);
        network_cleanup();
        return 1;
    }

    /* Create and bind server socket before reporting that the server is started */
    int server_fd = create_server_socket(port);
    if (server_fd < 0) {
        log_error("Failed to start server on port %d", port);
        catalog_free(&catalog);
        network_cleanup();
        return 1;
    }

    printf("Server started on port %d\n", port);
    printf("Shared files:\n");
    if (catalog.count == 0) {
        printf("  (No files found in shared directory)\n");
    } else {
        FileEntry *curr = catalog.head;
        int idx = 1;
        while (curr != NULL) {
            printf("%d. %s (%lld bytes)\n", idx++, curr->name, curr->size);
            curr = curr->next;
        }
    }
    printf("Waiting for incoming peer connections... (Ctrl+C to quit)\n");
    fflush(stdout);

    /* Register signal handlers for clean shutdown */
    signal(SIGINT, handle_sigint);
#ifdef SIGTERM
    signal(SIGTERM, handle_sigint);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    while (g_server_running) {
        PeerInfo client_peer;
        int client_fd = accept_client(server_fd, &client_peer);
        if (client_fd < 0) {
            if (!g_server_running) {
                break;
            }
            continue;
        }

        printf("\nConnection from ");
        peer_print(&client_peer);
        printf("\n");
        fflush(stdout);

        char filename[MAX_FILENAME + 1];
        if (receive_request(client_fd, filename, sizeof(filename)) != 0) {
            log_error("Failed to receive valid request from peer");
            close_socket(client_fd);
            continue;
        }

        printf("Request received for: %s\n", filename);
        fflush(stdout);

        /* Data Structures step: search the linked-list catalog */
        FileEntry *entry = catalog_find(&catalog, filename);
        if (entry == NULL) {
            printf("File not found in catalog: sending NOT_FOUND\n");
            fflush(stdout);
            send_response(client_fd, STATUS_NOT_FOUND, 0);
        } else {
            /* Verify the file can be opened for reading before sending STATUS_OK */
            FILE *check_fp = fopen(entry->path, "rb");
            if (!check_fp) {
                log_error("File '%s' cannot be opened for reading: sending NOT_FOUND", entry->path);
                send_response(client_fd, STATUS_NOT_FOUND, 0);
            } else {
                /* Refresh file size from disk in case it changed */
                fseek(check_fp, 0, SEEK_END);
                long long current_size = (long long)ftell(check_fp);
                fclose(check_fp);
                if (current_size >= 0) {
                    entry->size = current_size;
                }

                printf("File found: %s (%lld bytes). Sending file...\n", entry->name, entry->size);
                fflush(stdout);
                if (send_response(client_fd, STATUS_OK, (uint64_t)entry->size) == 0) {
                    if (send_file(client_fd, entry->path, entry->size) == 0) {
                        printf("File transfer complete for '%s'\n", entry->name);
                        fflush(stdout);
                    } else {
                        log_error("File transfer failed for '%s'", entry->name);
                    }
                } else {
                    log_error("Failed to send response header to peer");
                }
            }
        }

        close_socket(client_fd);
    }

    close_socket(server_fd);
    catalog_free(&catalog);
    network_cleanup();
    printf("Server stopped cleanly.\n");
    fflush(stdout);
    return 0;
}

/**
 * Runs the peer in client (requester) mode.
 * Connects to peer, sends request, receives file, reports timing & speed.
 */
static int run_client(const char *ip, int port, const char *filename, const char *output_path) {
    if (!is_valid_filename(filename)) {
        log_error("Invalid filename: '%s'", filename);
        return 1;
    }

    if (network_init() != 0) {
        log_error("Network initialization failed");
        return 1;
    }

    int sockfd = connect_to_peer(ip, port);
    if (sockfd < 0) {
        network_cleanup();
        return 1;
    }

    printf("Connected to %s:%d\n", ip, port);
    printf("Requesting: %s\n", filename);
    fflush(stdout);

    if (send_request(sockfd, filename) != 0) {
        log_error("Failed to send file request to peer");
        close_socket(sockfd);
        network_cleanup();
        return 1;
    }

    uint8_t status = 0;
    uint64_t file_size = 0;
    if (receive_response(sockfd, &status, &file_size) != 0) {
        log_error("Failed to receive response header from peer");
        close_socket(sockfd);
        network_cleanup();
        return 1;
    }

    if (status == STATUS_NOT_FOUND) {
        printf("File not found on peer\n");
        fflush(stdout);
        close_socket(sockfd);
        network_cleanup();
        return 1;
    }

    printf("File size: %lld bytes\n", (long long)file_size);
    fflush(stdout);

    double start_time = get_time_seconds();

    if (receive_file(sockfd, output_path, (long long)file_size) != 0) {
        log_error("File reception failed");
        close_socket(sockfd);
        network_cleanup();
        return 1;
    }

    double end_time = get_time_seconds();
    double elapsed = end_time - start_time;
    if (elapsed <= 0.000001) {
        elapsed = 0.000001; /* Prevent divide by zero */
    }

    char speed_str[32];
    format_speed((long long)file_size, elapsed, speed_str, sizeof(speed_str));

    printf("Transfer complete\n");
    printf("Saved to: %s\n", output_path);
    printf("Bytes received: %lld\n", (long long)file_size);
    printf("Time: %.3f seconds\n", elapsed);
    printf("Speed: %s\n", speed_str);
    fflush(stdout);

    close_socket(sockfd);
    network_cleanup();
    return 0;
}

/**
 * Runs the optional list command to inspect shared catalog locally without networking.
 */
static int run_list(const char *shared_dir) {
    FileCatalog catalog;
    catalog_init(&catalog);

    if (catalog_scan_directory(&catalog, shared_dir) != 0) {
        log_error("Could not scan directory '%s'", shared_dir);
        catalog_free(&catalog);
        return 1;
    }

    printf("Catalog inspection for directory: %s\n", shared_dir);
    catalog_print(&catalog);
    catalog_free(&catalog);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        int port = parse_port(argv[2]);
        if (port < 0) {
            log_error("Invalid port: '%s' (must be an integer between 1 and 65535)", argv[2]);
            return 1;
        }
        const char *shared_dir = argv[3];
        return run_server(port, shared_dir);
    }

    if (strcmp(argv[1], "get") == 0) {
        if (argc < 6) {
            print_usage(argv[0]);
            return 1;
        }
        const char *ip = argv[2];
        int port = parse_port(argv[3]);
        if (port < 0) {
            log_error("Invalid port: '%s' (must be an integer between 1 and 65535)", argv[3]);
            return 1;
        }
        const char *filename = argv[4];
        const char *output_path = argv[5];
        return run_client(ip, port, filename, output_path);
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc < 3) {
            print_usage(argv[0]);
            return 1;
        }
        const char *shared_dir = argv[2];
        return run_list(shared_dir);
    }

    print_usage(argv[0]);
    return 1;
}
