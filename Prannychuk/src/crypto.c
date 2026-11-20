#include "crypto.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define MAGIC_LEN 8u
#define SALT_LEN 16u
#define NONCE_LEN 12u
#define TAG_LEN 16u
#define KEY_LEN 32u
#define AAD_LEN (MAGIC_LEN + SALT_LEN + NONCE_LEN + 4u)
#define FILE_HEADER_LEN (AAD_LEN + TAG_LEN)

static const uint8_t MAGIC[MAGIC_LEN] = {'P','F','T','E','N','C','0','1'};

static void write_u32(uint8_t *out, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) {
        out[i] = (uint8_t)(v >> (i * 8u));
    }
}

static uint32_t read_u32(const uint8_t *in)
{
    uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i) {
        n |= (uint32_t)in[i] << (8u * i);
    }
    return n;
}

static int valid_password(const char *password)
{
    if (password == NULL) return 0;
    size_t n = strlen(password);
    return n >= 1 && n <= 1024 && n <= INT_MAX;
}

static int derive_key(const char *password, const uint8_t *salt, uint8_t key[KEY_LEN])
{
    return PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt,
                             SALT_LEN, PFT_KDF_ITERATIONS, EVP_sha256(),
                             KEY_LEN, key) == 1;
}

const char *pft_status_string(PFTStatus status)
{
    switch (status) {
        case PFT_OK:           return "OK";
        case PFT_ERR_ARGUMENT: return "Invalid argument or transaction";
        case PFT_ERR_MEMORY:   return "Out of memory";
        case PFT_ERR_IO:       return "File I/O error";
        case PFT_ERR_FORMAT:   return "Invalid or damaged data format";
        case PFT_ERR_CRYPTO:   return "Encryption library error";
        case PFT_ERR_AUTH:     return "Wrong password or modified encrypted file";
        default:               return "Unknown error";
    }
}

PFTStatus pft_encrypt_bytes(const uint8_t *plaintext, size_t length,
                            const char *password, uint8_t **out_data,
                            size_t *out_length)
{
    if (!out_data || !out_length) return PFT_ERR_ARGUMENT;
    *out_data = NULL;
    *out_length = 0;
    if ((!plaintext && length != 0) || !valid_password(password) ||
        length > PFT_MAX_PLAINTEXT || length > INT_MAX) return PFT_ERR_ARGUMENT;

    uint8_t *blob = malloc(FILE_HEADER_LEN + length);
    if (!blob) return PFT_ERR_MEMORY;
    memcpy(blob, MAGIC, MAGIC_LEN);
    uint8_t *salt = blob + MAGIC_LEN;
    uint8_t *nonce = salt + SALT_LEN;
    uint8_t *len_field = nonce + NONCE_LEN;
    uint8_t *tag = len_field + 4;
    uint8_t *ciphertext = tag + TAG_LEN;
    write_u32(len_field, (uint32_t)length);

    PFTStatus status = PFT_ERR_CRYPTO;
    uint8_t key[KEY_LEN] = {0};
    EVP_CIPHER_CTX *ctx = NULL;
    if (RAND_bytes(salt, SALT_LEN) != 1 || RAND_bytes(nonce, NONCE_LEN) != 1 ||
        !derive_key(password, salt, key)) goto done;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { status = PFT_ERR_MEMORY; goto done; }
    int n = 0, total = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce) != 1 ||
        EVP_EncryptUpdate(ctx, NULL, &n, blob, AAD_LEN) != 1) goto done;
    if (length && EVP_EncryptUpdate(ctx, ciphertext, &n, plaintext, (int)length) != 1) goto done;
    if (length) total = n;
    if (EVP_EncryptFinal_ex(ctx, ciphertext + total, &n) != 1) goto done;
    total += n;
    if ((size_t)total != length ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag) != 1) goto done;

    *out_data = blob;
    *out_length = FILE_HEADER_LEN + length;
    blob = NULL;
    status = PFT_OK;
done:
    OPENSSL_cleanse(key, sizeof key);
    EVP_CIPHER_CTX_free(ctx);
    if (blob) { OPENSSL_cleanse(blob, FILE_HEADER_LEN + length); free(blob); }
    return status;
}

PFTStatus pft_decrypt_bytes(const uint8_t *encrypted, size_t length,
                            const char *password, uint8_t **out_data,
                            size_t *out_length)
{
    if (!out_data || !out_length) return PFT_ERR_ARGUMENT;
    *out_data = NULL;
    *out_length = 0;
    if (!encrypted || !valid_password(password)) return PFT_ERR_ARGUMENT;
    if (length < FILE_HEADER_LEN || memcmp(encrypted, MAGIC, MAGIC_LEN) != 0)
        return PFT_ERR_FORMAT;
    uint32_t clen = read_u32(encrypted + MAGIC_LEN + SALT_LEN + NONCE_LEN);
    if (clen > PFT_MAX_PLAINTEXT || (size_t)clen != length - FILE_HEADER_LEN ||
        clen > INT_MAX) return PFT_ERR_FORMAT;

    const uint8_t *salt = encrypted + MAGIC_LEN;
    const uint8_t *nonce = salt + SALT_LEN;
    const uint8_t *tag = encrypted + AAD_LEN;
    const uint8_t *ciphertext = tag + TAG_LEN;
    uint8_t *plaintext = malloc((size_t)clen + 1u);
    if (!plaintext) return PFT_ERR_MEMORY;

    PFTStatus status = PFT_ERR_CRYPTO;
    uint8_t key[KEY_LEN] = {0};
    EVP_CIPHER_CTX *ctx = NULL;
    if (!derive_key(password, salt, key)) goto done;
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { status = PFT_ERR_MEMORY; goto done; }

    int n = 0, total = 0;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce) != 1 ||
        EVP_DecryptUpdate(ctx, NULL, &n, encrypted, AAD_LEN) != 1) goto done;
    if (clen && EVP_DecryptUpdate(ctx, plaintext, &n, ciphertext, (int)clen) != 1) goto done;
    if (clen) total = n;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, (void *)tag) != 1)
        goto done;
    if (EVP_DecryptFinal_ex(ctx, plaintext + total, &n) != 1) {
        status = PFT_ERR_AUTH;
        goto done;
    }
    total += n;
    if (total != (int)clen) goto done;
    plaintext[clen] = 0;
    *out_data = plaintext;
    *out_length = clen;
    plaintext = NULL;
    status = PFT_OK;
done:
    OPENSSL_cleanse(key, sizeof key);
    EVP_CIPHER_CTX_free(ctx);
    if (plaintext) { OPENSSL_cleanse(plaintext, (size_t)clen + 1u); free(plaintext); }
    return status;
}
