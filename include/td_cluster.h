#ifndef TD_CLUSTER_H
#define TD_CLUSTER_H

#include "td_transport.h"

typedef struct {
    td_config_t config;
    td_crypto_ctx_t crypto;
    td_session_t sessions[TD_MAX_ENDPOINTS];
    size_t session_count;
} td_cluster_t;

typedef struct {
    uint64_t total_ns;
    td_transport_t transport;
    td_transport_profile_t transport_profile;
    uint64_t hash_ns;
    uint64_t prime_probe_ns;
    uint64_t prime_probe_scan_ns;
    uint64_t cache_probe_ns;
    uint64_t cache_probe_scan_ns;
    uint64_t cache_decode_ns;
    uint64_t prime_decode_ns;
    uint64_t cache_validation_ns;
    uint64_t crypto_encode_ns;
    uint64_t refresh_cache_probe_ns;
    uint64_t refresh_cache_probe_scan_ns;
    uint64_t refresh_cache_write_ns;
    uint64_t refresh_cache_cas_ns;
    uint64_t backup_probe_ns;
    uint64_t backup_probe_scan_ns;
    uint64_t backup_write_ns;
    uint64_t backup_cas_ns;
    uint64_t rule_eval_ns;
    uint64_t wait_for_primary_change_ns;
    uint64_t primary_write_ns;
    uint64_t primary_cas_ns;
    uint64_t repair_probe_ns;
    uint64_t repair_probe_scan_ns;
    uint64_t repair_write_ns;
    uint64_t repair_cas_ns;
    uint64_t crypto_slot_hash_ns;
    uint64_t crypto_tie_breaker_ns;
    uint64_t crypto_iv_ns;
    uint64_t crypto_mac_setup_ns;
    uint64_t crypto_mac_body_ns;
    uint64_t crypto_mac_finalize_ns;
    uint64_t crypto_encrypt_setup_ns;
    uint64_t crypto_mac_ns;
    uint64_t crypto_verify_mac_ns;
    uint64_t crypto_encrypt_ns;
    uint64_t crypto_decrypt_setup_ns;
    uint64_t crypto_decrypt_ns;
    size_t prime_probe_reads;
    size_t prime_probe_slots_examined;
    size_t prime_probe_guard_mismatch;
    size_t prime_probe_tombstones;
    size_t prime_probe_empty_hits;
    size_t cache_probe_reads;
    size_t cache_probe_slots_examined;
    size_t cache_probe_guard_mismatch;
    size_t cache_probe_tombstones;
    size_t cache_probe_empty_hits;
    size_t backup_probe_reads;
    size_t backup_probe_slots_examined;
    size_t backup_probe_guard_mismatch;
    size_t backup_probe_tombstones;
    size_t backup_probe_empty_hits;
    size_t repair_probe_reads;
    size_t repair_probe_slots_examined;
    size_t repair_probe_guard_mismatch;
    size_t repair_probe_tombstones;
    size_t repair_probe_empty_hits;
    size_t refresh_cache_probe_reads;
    size_t refresh_cache_probe_slots_examined;
    size_t refresh_cache_probe_guard_mismatch;
    size_t refresh_cache_probe_tombstones;
    size_t refresh_cache_probe_empty_hits;
    size_t backup_targets;
    size_t backup_successes;
    size_t repair_attempts;
    size_t wait_for_primary_change_attempts;
    int cache_enabled;
    int cache_hit;
} td_latency_profile_t;

int td_cluster_init(td_cluster_t *cluster, const td_config_t *cfg, char *err, size_t err_len);
void td_cluster_close(td_cluster_t *cluster);
int td_cluster_execute(td_cluster_t *cluster, const char *line, FILE *out);
void td_cluster_print_status(td_cluster_t *cluster, FILE *out);
int td_cluster_read_kv(td_cluster_t *cluster, const char *key, unsigned char *value, size_t *value_len, int *found, char *err, size_t err_len);
int td_cluster_write_kv(td_cluster_t *cluster, const char *key, const unsigned char *value, size_t value_len, int *rule_out, char *err, size_t err_len);
int td_cluster_update_kv(td_cluster_t *cluster, const char *key, const unsigned char *value, size_t value_len, int *rule_out, char *err, size_t err_len);
int td_cluster_delete_kv(td_cluster_t *cluster, const char *key, int *rule_out, char *err, size_t err_len);
int td_cluster_read_kv_profiled(td_cluster_t *cluster, const char *key, unsigned char *value, size_t *value_len, int *found, td_latency_profile_t *profile, char *err, size_t err_len);
int td_cluster_write_kv_profiled(td_cluster_t *cluster, const char *key, const unsigned char *value, size_t value_len, int *rule_out, td_latency_profile_t *profile, char *err, size_t err_len);
int td_cluster_update_kv_profiled(td_cluster_t *cluster, const char *key, const unsigned char *value, size_t value_len, int *rule_out, td_latency_profile_t *profile, char *err, size_t err_len);
int td_cluster_delete_kv_profiled(td_cluster_t *cluster, const char *key, int *rule_out, td_latency_profile_t *profile, char *err, size_t err_len);

#endif
