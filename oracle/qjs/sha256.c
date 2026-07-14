/* oracle/qjs/sha256.c — see sha256.h for provenance + role. */
#include "sha256.h"

#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(sha256_ctx *c, const uint8_t p[64]) {
    uint32_t w[64];
    uint32_t a, b, d, e, f, g, h0, h1, t1, t2;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0]; b = c->h[1]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6];
    h0 = c->h[2]; h1 = c->h[7];
    /* h0 doubles as "c" (the working var), h1 as "h" — avoid shadowing the
     * ctx parameter name. Standard compression loop otherwise. */
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & h0) ^ (b & h0);
        t1 = h1 + S1 + ch + K[i] + w[i];
        t2 = S0 + maj;
        h1 = g; g = f; f = e; e = d + t1;
        d = h0; h0 = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += h0; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h1;
}

void sha256_init(sha256_ctx *c) {
    c->h[0] = 0x6a09e667; c->h[1] = 0xbb67ae85;
    c->h[2] = 0x3c6ef372; c->h[3] = 0xa54ff53a;
    c->h[4] = 0x510e527f; c->h[5] = 0x9b05688c;
    c->h[6] = 0x1f83d9ab; c->h[7] = 0x5be0cd19;
    c->len = 0;
    c->buflen = 0;
}

void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len) {
    c->len += len;
    if (c->buflen) {
        size_t need = 64 - c->buflen;
        size_t take = len < need ? len : need;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take;
        data += take;
        len -= take;
        if (c->buflen == 64) {
            sha256_block(c, c->buf);
            c->buflen = 0;
        }
    }
    while (len >= 64) {
        sha256_block(c, data);
        data += 64;
        len -= 64;
    }
    if (len) {
        memcpy(c->buf, data, len);
        c->buflen = len;
    }
}

void sha256_final(sha256_ctx *c, uint8_t out[32]) {
    uint64_t bitlen = c->len * 8;
    uint8_t pad = 0x80;
    uint8_t zero = 0;
    int i;
    sha256_update(c, &pad, 1);
    while (c->buflen != 56)
        sha256_update(c, &zero, 1);
    for (i = 7; i >= 0; i--) {
        uint8_t b = (uint8_t)(bitlen >> (i * 8));
        sha256_update(c, &b, 1);
    }
    /* the length bytes complete the final block, so buflen is 0 here */
    for (i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(c->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(c->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(c->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(c->h[i]);
    }
}

void sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, len);
    sha256_final(&c, out);
}

/* FIPS 180-4 / NIST CAVP reference digests */
static int check(const char *msg, size_t len, const char *hexdigest) {
    static const char *hx = "0123456789abcdef";
    uint8_t d[32];
    char got[65];
    int i;
    sha256((const uint8_t *)msg, len, d);
    for (i = 0; i < 32; i++) {
        got[i * 2] = hx[d[i] >> 4];
        got[i * 2 + 1] = hx[d[i] & 15];
    }
    got[64] = 0;
    return strcmp(got, hexdigest) != 0;
}

int sha256_self_test(void) {
    int rc = 0;
    rc |= check("", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    rc |= check("abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    rc |= check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    {
        /* 1,000,000 x 'a', streamed in odd-sized chunks to exercise
         * the buffering paths */
        sha256_ctx c;
        uint8_t d[32];
        uint8_t chunk[137];
        size_t left = 1000000;
        char got[65];
        static const char *hx = "0123456789abcdef";
        int i;
        memset(chunk, 'a', sizeof(chunk));
        sha256_init(&c);
        while (left) {
            size_t take = left < sizeof(chunk) ? left : sizeof(chunk);
            sha256_update(&c, chunk, take);
            left -= take;
        }
        sha256_final(&c, d);
        for (i = 0; i < 32; i++) {
            got[i * 2] = hx[d[i] >> 4];
            got[i * 2 + 1] = hx[d[i] & 15];
        }
        got[64] = 0;
        rc |= strcmp(got,
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") != 0;
    }
    return rc;
}
