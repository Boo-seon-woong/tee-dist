#include "td_transport.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t td_stop = 0;

typedef struct {
    td_local_region_t *region;
    size_t threshold_pct;
} td_eviction_ctx_t;

static void td_signal_handler(int signo) {
    (void)signo;
    td_stop = 1;
}

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

static void *td_eviction_thread(void *arg) {
    td_eviction_ctx_t *ctx = (td_eviction_ctx_t *)arg;
    while (!td_stop) {
        td_region_evict_if_needed(ctx->region, ctx->threshold_pct);
        usleep(100000);
    }
    return NULL;
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    td_config_t cfg;
    td_local_region_t region;
    td_eviction_ctx_t eviction_ctx;
    char err[256];
    pthread_t evict_thread;
    int rc;

    if (td_find_config(argc, argv, &config_path) != 0) {
        fprintf(stderr, "usage: %s --config build/config/mn1.conf\n", argv[0]);
        return 1;
    }
    if (td_config_load(config_path, &cfg, err, sizeof(err)) != 0) {
        fprintf(stderr, "config error: %s\n", err);
        return 1;
    }
    if (cfg.mode != TD_MODE_MN) {
        fprintf(stderr, "config error: mode must be mn\n");
        return 1;
    }
    if (td_region_open(&region, &cfg, err, sizeof(err)) != 0) {
        fprintf(stderr, "region error: %s\n", err);
        return 1;
    }

    signal(SIGINT, td_signal_handler);
    signal(SIGTERM, td_signal_handler);
    eviction_ctx.region = &region;
    eviction_ctx.threshold_pct = cfg.eviction_threshold_pct;
    pthread_create(&evict_thread, NULL, td_eviction_thread, &eviction_ctx);

    fprintf(stdout, "tee-dist mn node_id=%d transport=%s listen=%s:%d backing=%s bytes=%llu\n",
        cfg.node_id,
        cfg.transport == TD_TRANSPORT_RDMA ? "rdma" : "tcp",
        cfg.listen_host,
        cfg.listen_port,
        cfg.memory_file,
        (unsigned long long)region.mapped_bytes);
    fflush(stdout);

    if (cfg.transport == TD_TRANSPORT_RDMA) {
        rc = td_rdma_server_run(&cfg, &region, &td_stop, err, sizeof(err));
    } else {
        rc = td_tcp_server_run(&cfg, &region, &td_stop, err, sizeof(err));
    }

    td_stop = 1;
    pthread_join(evict_thread, NULL);
    td_region_close(&region);

    if (rc != 0) {
        fprintf(stderr, "server error: %s\n", err);
        return 1;
    }
    return 0;
}
