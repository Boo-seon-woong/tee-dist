#ifndef TD_CRYPTO_H
#define TD_CRYPTO_H

#include "td_layout.h"

typedef struct {
    unsigned char enc_key[32];
    unsigned char mac_key[32];
} td_crypto_ctx_t;

typedef struct {
    uint64_t slot_hash_ns;
    uint64_t tie_breaker_ns;
    uint64_t iv_ns;
    uint64_t mac_setup_ns;
    uint64_t mac_body_ns;
    uint64_t mac_finalize_ns;
    uint64_t encrypt_setup_ns;
    uint64_t mac_ns;
    uint64_t verify_mac_ns;
    uint64_t encrypt_ns;
    uint64_t decrypt_setup_ns;
    uint64_t decrypt_ns;
} td_crypto_profile_t;

int td_crypto_init(td_crypto_ctx_t *ctx, const char *hex_key, char *err, size_t err_len);
uint64_t td_crypto_tie_breaker(const char *logical_key, const unsigned char *value, size_t value_len, uint64_t epoch);
int td_crypto_make_slot(
    const td_crypto_ctx_t *ctx,
    const char *logical_key,
    const unsigned char *value,
    size_t value_len,
    uint32_t flags,
    uint64_t epoch,
    td_slot_t *slot);
int td_crypto_make_slot_profiled(
    const td_crypto_ctx_t *ctx,
    const char *logical_key,
    const unsigned char *value,
    size_t value_len,
    uint32_t flags,
    uint64_t epoch,
    td_slot_t *slot,
    td_crypto_profile_t *profile);
int td_crypto_decode_slot(
    const td_crypto_ctx_t *ctx,
    const char *logical_key,
    const td_slot_t *slot,
    unsigned char *plaintext,
    size_t *plaintext_len);
int td_crypto_decode_slot_profiled(
    const td_crypto_ctx_t *ctx,
    const char *logical_key,
    const td_slot_t *slot,
    unsigned char *plaintext,
    size_t *plaintext_len,
    td_crypto_profile_t *profile);

#endif
