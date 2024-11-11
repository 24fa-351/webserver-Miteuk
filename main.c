#include "server.h"

#define DEFAULT_PORT 80

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    start_server(port);
    return 0;
}