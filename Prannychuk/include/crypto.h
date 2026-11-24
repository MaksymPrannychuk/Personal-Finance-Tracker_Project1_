#ifndef PFT_CRYPTO_H
#define PFT_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

/* Public format uses 250,000 PBKDF2-HMAC-SHA256 rounds, AES-256-GCM,
   16-byte random salt, 12-byte random nonce and 16-byte authentication tag. */
#define PFT_KDF_ITERATIONS 250000
#define PFT_MAX_PLAINTEXT (16u * 1024u * 1024u)

typedef enum {
    PFT_OK = 0,
    PFT_ERR_ARGUMENT,
    PFT_ERR_MEMORY,
    PFT_ERR_IO,
    PFT_ERR_FORMAT,
    PFT_ERR_CRYPTO,
    PFT_ERR_AUTH
} PFTStatus;

/* Output allocations belong to the caller (free them after use).
   After decrypting, use OPENSSL_cleanse() on sensitive plaintext before free(). */
PFTStatus pft_encrypt_bytes(const uint8_t *plaintext, size_t length,
                            const char *password, uint8_t **out_data,
                            size_t *out_length);
PFTStatus pft_decrypt_bytes(const uint8_t *encrypted, size_t length,
                            const char *password, uint8_t **out_data,
                            size_t *out_length);
const char *pft_status_string(PFTStatus status);

#endif
