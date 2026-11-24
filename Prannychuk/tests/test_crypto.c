#include "crypto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void roundtrip(void)
{
    const uint8_t text[] = "Hello! A private financial record";
    uint8_t *encrypted = NULL, *decrypted = NULL;
    size_t elen = 0, dlen = 0;
    assert(pft_encrypt_bytes(text, sizeof text, "example-password", &encrypted, &elen) == PFT_OK);
    assert(elen > sizeof text);
    assert(pft_decrypt_bytes(encrypted, elen, "example-password", &decrypted, &dlen) == PFT_OK);
    assert(dlen == sizeof text && memcmp(decrypted, text, dlen) == 0);
    free(decrypted);
    free(encrypted);
    puts("[PASS] Crypto: encrypt / decrypt roundtrip");
}

static void wrong_password_and_modified_data(void)
{
    const uint8_t text[] = {1,2,3,4,5,6};
    uint8_t *cipher = NULL, *out = NULL;
    size_t len = 0, outlen = 0;
    assert(pft_encrypt_bytes(text, sizeof text, "correct", &cipher, &len) == PFT_OK);
    assert(pft_decrypt_bytes(cipher, len, "wrong", &out, &outlen) == PFT_ERR_AUTH);
    assert(out == NULL);
    cipher[len - 1] ^= 0x01;
    assert(pft_decrypt_bytes(cipher, len, "correct", &out, &outlen) == PFT_ERR_AUTH);
    assert(out == NULL);
    free(cipher);
    puts("[PASS] Crypto: wrong password and tampering rejected");
}

static void random_nonce_and_salt(void)
{
    uint8_t *a = NULL, *b = NULL;
    size_t na = 0, nb = 0;
    const uint8_t plain[4] = {4,3,2,1};
    assert(pft_encrypt_bytes(plain, sizeof plain, "p", &a, &na) == PFT_OK);
    assert(pft_encrypt_bytes(plain, sizeof plain, "p", &b, &nb) == PFT_OK);
    assert(na == nb && memcmp(a, b, na) != 0);
    free(a); free(b);
    puts("[PASS] Crypto: different salt/nonce on every encryption");
}

static void empty_and_invalid(void)
{
    uint8_t *a = NULL, *b = NULL;
    size_t na = 0, nb = 0;
    assert(pft_encrypt_bytes(NULL, 0, "p", &a, &na) == PFT_OK);
    assert(pft_decrypt_bytes(a, na, "p", &b, &nb) == PFT_OK && nb == 0);
    free(a); free(b);
    assert(pft_encrypt_bytes((const uint8_t *)"A", 1, "", &a, &na) == PFT_ERR_ARGUMENT);
    assert(pft_decrypt_bytes((const uint8_t *)"BAD", 3, "p", &b, &nb) == PFT_ERR_FORMAT);
    puts("[PASS] Crypto: empty messages and invalid arguments");
}

int main(void)
{
    roundtrip();
    wrong_password_and_modified_data();
    random_nonce_and_salt();
    empty_and_invalid();
    puts("All 4 crypto tests passed.");
    return 0;
}
