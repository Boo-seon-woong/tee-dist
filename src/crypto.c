#include "td_crypto.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <string.h>

static void td_sha256_two_parts(const unsigned char *a, size_t a_len, const unsigned char *b, size_t b_len, unsigned char out[SHA256_DIGEST_LENGTH]) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, a, a_len);
    SHA256_Update(&ctx, b, b_len);
    SHA256_Final(out, &ctx);
}

static int td_crypto_mac(const td_crypto_ctx_t *ctx, const td_slot_t *slot, unsigned char out[32]) {
    unsigned int out_len = 0;
    HMAC_CTX *hctx = HMAC_CTX_new();
    if (hctx == NULL) {
        return -1;
    }
    if (HMAC_Init_ex(hctx, ctx->mac_key, sizeof(ctx->mac_key), EVP_sha256(), NULL) != 1 ||
        HMAC_Update(hctx, (const unsigned char *)&slot->key_hash, sizeof(slot->key_hash)) != 1 ||
        HMAC_Update(hctx, (const unsigned char *)&slot->visible_epoch, sizeof(slot->visible_epoch)) != 1 ||
        HMAC_Update(hctx, (const unsigned char *)&slot->tie_breaker, sizeof(slot->tie_breaker)) != 1 ||
        HMAC_Update(hctx, (const unsigned char *)&slot->flags, sizeof(slot->flags)) != 1 ||
        HMAC_Update(hctx, (const unsigned char *)&slot->value_len, sizeof(slot->value_len)) != 1 ||
        HMAC_Update(hctx, slot->iv, sizeof(slot->iv)) != 1 ||
        HMAC_Update(hctx, slot->ciphertext, slot->value_len) != 1 ||
        HMAC_Final(hctx, out, &out_len) != 1) {
        HMAC_CTX_free(hctx);
        return -1;
    }
    HMAC_CTX_free(hctx);
    return out_len == 32 ? 0 : -1;
}

int td_crypto_init(td_crypto_ctx_t *ctx, const char *hex_key, char *err, size_t err_len) {
    unsigned char raw[TD_KEY_MATERIAL_BYTES];
    unsigned char enc_seed[SHA256_DIGEST_LENGTH];
    unsigned char mac_seed[SHA256_DIGEST_LENGTH];
    static const unsigned char enc_label[] = "tee-dist-enc";
    static const unsigned char mac_label[] = "tee-dist-mac";

    memset(ctx, 0, sizeof(*ctx));
    if (td_hex_to_bytes(hex_key, raw, sizeof(raw)) != 0) {
        td_format_error(err, err_len, "encryption_key_hex must be exactly %d hex chars", TD_KEY_MATERIAL_BYTES * 2);
        return -1;
    }
    td_sha256_two_parts(raw, sizeof(raw), enc_label, sizeof(enc_label) - 1, enc_seed);
    td_sha256_two_parts(raw, sizeof(raw), mac_label, sizeof(mac_label) - 1, mac_seed);
    memcpy(ctx->enc_key, enc_seed, sizeof(ctx->enc_key));
    memcpy(ctx->mac_key, mac_seed, sizeof(ctx->mac_key));
    return 0;
}

uint64_t td_crypto_tie_breaker(const char *logical_key, const unsigned char *value, size_t value_len, uint64_t epoch) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, logical_key, strlen(logical_key));
    SHA256_Update(&ctx, value, value_len);
    SHA256_Update(&ctx, &epoch, sizeof(epoch));
    SHA256_Final(digest, &ctx);
    return ((uint64_t)digest[0] << 56) |
           ((uint64_t)digest[1] << 48) |
           ((uint64_t)digest[2] << 40) |
           ((uint64_t)digest[3] << 32) |
           ((uint64_t)digest[4] << 24) |
           ((uint64_t)digest[5] << 16) |
           ((uint64_t)digest[6] << 8) |
           (uint64_t)digest[7];
}

int td_crypto_make_slot(
    const td_crypto_ctx_t *ctx,
    const char *logical_key,
    const unsigned char *value,
    size_t value_len,
    uint32_t flags,
    uint64_t epoch,
    td_slot_t *slot) {
    EVP_CIPHER_CTX *cipher;
    unsigned char iv_seed[SHA256_DIGEST_LENGTH];
    int out_len = 0;
    int tmp_len = 0;

    if (value_len > TD_MAX_VALUE_SIZE) {
        return -1;
    }

    memset(slot, 0, sizeof(*slot));
    slot->guard_epoch = epoch;
    slot->visible_epoch = epoch;
    slot->key_hash = td_hash64_string(logical_key);
    slot->tie_breaker = td_crypto_tie_breaker(logical_key, value, value_len, epoch);
    slot->flags = flags;
    slot->value_len = (uint32_t)value_len;

    td_sha256_two_parts((const unsigned char *)logical_key, strlen(logical_key), (const unsigned char *)&epoch, sizeof(epoch), iv_seed);
    memcpy(slot->iv, iv_seed, sizeof(slot->iv));

    cipher = EVP_CIPHER_CTX_new();
    if (cipher == NULL) {
        return -1;
    }
    if (EVP_EncryptInit_ex(cipher, EVP_aes_256_ctr(), NULL, ctx->enc_key, slot->iv) != 1 ||
        EVP_EncryptUpdate(cipher, slot->ciphertext, &out_len, value, (int)value_len) != 1 ||
        EVP_EncryptFinal_ex(cipher, slot->ciphertext + out_len, &tmp_len) != 1) {
        EVP_CIPHER_CTX_free(cipher);
        return -1;
    }
    EVP_CIPHER_CTX_free(cipher);
    slot->value_len = (uint32_t)(out_len + tmp_len);

    if (td_crypto_mac(ctx, slot, slot->mac) != 0) {
        return -1;
    }
    return 0;
}

int td_crypto_decode_slot(
    const td_crypto_ctx_t *ctx,
    const char *logical_key,
    const td_slot_t *slot,
    unsigned char *plaintext,
    size_t *plaintext_len) {
    EVP_CIPHER_CTX *cipher;
    unsigned char mac[32];
    int out_len = 0;
    int tmp_len = 0;

    (void)logical_key;

    if (slot->value_len > TD_MAX_VALUE_SIZE) {
        return -1;
    }
    if (slot->guard_epoch != slot->visible_epoch) {
        return -1;
    }
    if (td_crypto_mac(ctx, slot, mac) != 0) {
        return -1;
    }
    if (memcmp(mac, slot->mac, sizeof(mac)) != 0) {
        return -1;
    }
    if ((slot->flags & TD_SLOT_FLAG_VALID) == 0) {
        return -1;
    }

    cipher = EVP_CIPHER_CTX_new();
    if (cipher == NULL) {
        return -1;
    }
    if (EVP_DecryptInit_ex(cipher, EVP_aes_256_ctr(), NULL, ctx->enc_key, slot->iv) != 1 ||
        EVP_DecryptUpdate(cipher, plaintext, &out_len, slot->ciphertext, (int)slot->value_len) != 1 ||
        EVP_DecryptFinal_ex(cipher, plaintext + out_len, &tmp_len) != 1) {
        EVP_CIPHER_CTX_free(cipher);
        return -1;
    }
    EVP_CIPHER_CTX_free(cipher);
    *plaintext_len = (size_t)(out_len + tmp_len);
    return 0;
}
