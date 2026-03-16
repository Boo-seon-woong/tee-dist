#include "td_cluster.h"

#include <stdio.h>
#include <string.h>

static int td_find_config(int argc, char **argv, const char **path) {
    int i;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            *path = argv[i + 1];
            return 0;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    td_config_t cfg;
    td_cluster_t cluster;
    char err[256];
    char line[TD_CMD_BYTES];

    if (td_find_config(argc, argv, &config_path) != 0) {
        fprintf(stderr, "usage: %s --config build/config/cn.conf\n", argv[0]);
        return 1;
    }
    if (td_config_load(config_path, &cfg, err, sizeof(err)) != 0) {
        fprintf(stderr, "config error: %s\n", err);
        return 1;
    }
    if (cfg.mode != TD_MODE_CN) {
        fprintf(stderr, "config error: mode must be cn\n");
        return 1;
    }
    if (td_cluster_init(&cluster, &cfg, err, sizeof(err)) != 0) {
        fprintf(stderr, "startup error: %s\n", err);
        return 1;
    }

    printf("tee-dist cn ready. type help for commands.\n");
    while (1) {
        printf("td> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        if (td_cluster_execute(&cluster, line, stdout) == 0) {
            break;
        }
    }

    td_cluster_close(&cluster);
    return 0;
}
