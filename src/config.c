#include "td_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int td_parse_mode(const char *value, td_mode_t *out) {
    if (strcmp(value, "cn") == 0) {
        *out = TD_MODE_CN;
        return 0;
    }
    if (strcmp(value, "mn") == 0) {
        *out = TD_MODE_MN;
        return 0;
    }
    return -1;
}

static int td_parse_transport(const char *value, td_transport_t *out) {
    if (strcmp(value, "tcp") == 0) {
        *out = TD_TRANSPORT_TCP;
        return 0;
    }
    if (strcmp(value, "rdma") == 0) {
        *out = TD_TRANSPORT_RDMA;
        return 0;
    }
    return -1;
}

static int td_parse_toggle(const char *value, int *out) {
    if (strcmp(value, "on") == 0) {
        *out = 1;
        return 0;
    }
    if (strcmp(value, "off") == 0) {
        *out = 0;
        return 0;
    }
    return -1;
}

static char *td_normalize_value(char *value) {
    size_t len;

    value = td_trim(value);
    len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        value[len - 1] = '\0';
        ++value;
    }
    return td_trim(value);
}

void td_config_init_defaults(td_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->mode = TD_MODE_CN;
    cfg->transport = TD_TRANSPORT_TCP;
    cfg->replication = 3;
    cfg->tdx = TD_TDX_OFF;
    cfg->cache = TD_CACHE_ON;
    cfg->mn_memory_size_mb = 64;
    cfg->rdma_gid_index = 0;
    cfg->listen_port = 0;
    cfg->node_id = -1;
    cfg->prime_slots = 1024;
    cfg->cache_slots = 256;
    cfg->backup_slots = 1024;
    cfg->max_value_size = TD_MAX_VALUE_SIZE;
    cfg->eviction_threshold_pct = 80;
    cfg->recv_queue_depth = TD_RECV_QUEUE_DEPTH;
    snprintf(cfg->rdma_device, sizeof(cfg->rdma_device), "%s", "mlx5_0");
    snprintf(cfg->listen_host, sizeof(cfg->listen_host), "%s", "0.0.0.0");
    snprintf(cfg->memory_file, sizeof(cfg->memory_file), "%s", "/tmp/tee-dist-mn.dat");
}

int td_config_load(const char *path, td_config_t *cfg, char *err, size_t err_len) {
    FILE *fp;
    char line[512];
    size_t line_no = 0;

    td_config_init_defaults(cfg);
    fp = fopen(path, "r");
    if (fp == NULL) {
        td_format_error(err, err_len, "cannot open config %s", path);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *key;
        char *value;
        char *sep;

        ++line_no;
        key = td_trim(line);
        if (*key == '\0' || *key == '#') {
            continue;
        }

        sep = strchr(key, ':');
        if (sep == NULL) {
            td_format_error(err, err_len, "config parse error at line %zu", line_no);
            fclose(fp);
            return -1;
        }

        *sep = '\0';
        value = td_normalize_value(sep + 1);
        key = td_trim(key);

        if (strcmp(key, "mode") == 0) {
            if (td_parse_mode(value, &cfg->mode) != 0) {
                td_format_error(err, err_len, "invalid mode at line %zu", line_no);
                fclose(fp);
                return -1;
            }
        } else if (strcmp(key, "transport") == 0) {
            if (td_parse_transport(value, &cfg->transport) != 0) {
                td_format_error(err, err_len, "invalid transport at line %zu", line_no);
                fclose(fp);
                return -1;
            }
        } else if (strcmp(key, "replication") == 0) {
            cfg->replication = atoi(value);
        } else if (strcmp(key, "tdx") == 0) {
            int toggle = 0;
            if (td_parse_toggle(value, &toggle) != 0) {
                td_format_error(err, err_len, "invalid tdx at line %zu", line_no);
                fclose(fp);
                return -1;
            }
            cfg->tdx = toggle ? TD_TDX_ON : TD_TDX_OFF;
        } else if (strcmp(key, "cache") == 0) {
            int toggle = 0;
            if (td_parse_toggle(value, &toggle) != 0) {
                td_format_error(err, err_len, "invalid cache at line %zu", line_no);
                fclose(fp);
                return -1;
            }
            cfg->cache = toggle ? TD_CACHE_ON : TD_CACHE_OFF;
        } else if (strcmp(key, "mn_memory_size_mb") == 0) {
            cfg->mn_memory_size_mb = (size_t)strtoull(value, NULL, 10);
        } else if (strcmp(key, "encryption_key_hex") == 0) {
            snprintf(cfg->encryption_key_hex, sizeof(cfg->encryption_key_hex), "%s", value);
        } else if (strcmp(key, "rdma_device") == 0) {
            snprintf(cfg->rdma_device, sizeof(cfg->rdma_device), "%s", value);
        } else if (strcmp(key, "rdma_gid_index") == 0) {
            cfg->rdma_gid_index = atoi(value);
        } else if (strcmp(key, "listen_host") == 0) {
            snprintf(cfg->listen_host, sizeof(cfg->listen_host), "%s", value);
        } else if (strcmp(key, "listen_port") == 0) {
            cfg->listen_port = atoi(value);
        } else if (strcmp(key, "node_id") == 0) {
            cfg->node_id = atoi(value);
        } else if (strcmp(key, "memory_file") == 0) {
            snprintf(cfg->memory_file, sizeof(cfg->memory_file), "%s", value);
        } else if (strcmp(key, "prime_slots") == 0) {
            cfg->prime_slots = (size_t)strtoull(value, NULL, 10);
        } else if (strcmp(key, "cache_slots") == 0) {
            cfg->cache_slots = (size_t)strtoull(value, NULL, 10);
        } else if (strcmp(key, "backup_slots") == 0) {
            cfg->backup_slots = (size_t)strtoull(value, NULL, 10);
        } else if (strcmp(key, "max_value_size") == 0) {
            cfg->max_value_size = (size_t)strtoull(value, NULL, 10);
        } else if (strcmp(key, "eviction_threshold_pct") == 0) {
            cfg->eviction_threshold_pct = (size_t)strtoull(value, NULL, 10);
        } else if (strcmp(key, "recv_queue_depth") == 0) {
            cfg->recv_queue_depth = (size_t)strtoull(value, NULL, 10);
        } else if (strcmp(key, "mn_endpoint") == 0) {
            if (cfg->mn_count >= TD_MAX_ENDPOINTS) {
                td_format_error(err, err_len, "too many mn_endpoint values");
                fclose(fp);
                return -1;
            }
            if (td_parse_host_port(value, &cfg->mn_endpoints[cfg->mn_count]) != 0) {
                td_format_error(err, err_len, "invalid mn_endpoint at line %zu", line_no);
                fclose(fp);
                return -1;
            }
            cfg->mn_endpoints[cfg->mn_count].node_id = (int)cfg->mn_count;
            ++cfg->mn_count;
        }
    }

    fclose(fp);

    if (cfg->max_value_size > TD_MAX_VALUE_SIZE) {
        td_format_error(err, err_len, "max_value_size exceeds built-in limit %d", TD_MAX_VALUE_SIZE);
        return -1;
    }
    if (cfg->replication <= 0) {
        cfg->replication = 1;
    }
    if (cfg->mode == TD_MODE_CN && cfg->mn_count == 0) {
        td_format_error(err, err_len, "cn config requires at least one mn_endpoint");
        return -1;
    }
    if (cfg->mode == TD_MODE_MN && cfg->listen_port <= 0) {
        td_format_error(err, err_len, "mn config requires listen_port");
        return -1;
    }
    if (cfg->encryption_key_hex[0] == '\0') {
        td_format_error(err, err_len, "encryption_key_hex must be set");
        return -1;
    }
    return 0;
}
