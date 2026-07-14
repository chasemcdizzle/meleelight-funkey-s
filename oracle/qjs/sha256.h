/*
 * oracle/qjs/sha256.h — SHA-256 (FIPS 180-4) for the QuickJS oracle
 * runtime (M0 task 6).
 *
 * Written for this project directly from the FIPS 180-4 specification
 * (standard K constants / initial H values); no third-party code vendored.
 * The embedder runs sha256_self_test() at startup and refuses to run if
 * the NIST test vectors fail — digest bytes MUST match WebCrypto's
 * SHA-256 exactly (oracle/CHECKSUM.md §4).
 */
#ifndef ORACLE_QJS_SHA256_H_
#define ORACLE_QJS_SHA256_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint64_t len;        /* total bytes fed */
    uint8_t buf[64];
    size_t buflen;
} sha256_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx *c, uint8_t out[32]);

/* one-shot */
void sha256(const uint8_t *data, size_t len, uint8_t out[32]);

/* returns 0 on success, nonzero on failure (NIST vectors: "", "abc",
 * "abcdbcde...", 1M x 'a') */
int sha256_self_test(void);

#endif /* ORACLE_QJS_SHA256_H_ */
