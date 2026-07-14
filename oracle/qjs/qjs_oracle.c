/*
 * oracle/qjs/qjs_oracle.c — the fdlibm-patched QuickJS oracle runtime
 * (M0 task 6; fix_plan §M0.6, PLAN §2-3).
 *
 * A qjsmin-style embedder (lineage: spikes/device-feasibility/qjsmin.c)
 * that proves the meleelight sim replays bit-identically OUTSIDE a
 * browser, on a runtime we control, with our vendored fdlibm as the ONLY
 * implementation of the locked Math surface:
 *
 *   - At startup, BEFORE any JS runs, the global Math table's
 *     sin/cos/tan/atan/atan2/pow are repointed at port/fdlibm/fdlibm.c
 *     (fd_sin & co, compiled with -ffp-contract=off). Argument coercion
 *     goes through JS_ToFloat64 (ECMAScript ToNumber), so JS semantics
 *     for non-number args (undefined -> NaN etc.) are preserved.
 *   - __qjs_sha256(typedArray|ArrayBuffer) -> ArrayBuffer(32): SHA-256
 *     for the per-frame state hash (oracle/CHECKSUM.md §4). QuickJS has
 *     no WebCrypto; sha256.c self-tests against NIST vectors at startup
 *     and the digest BYTES match crypto.subtle.digest("SHA-256", ...).
 *     (Namespaced __qjs_: pagelib.js defines window.__sha256 itself.)
 *   - __evalFile(path): evaluate a file in the global scope (the shim,
 *     the harness init/pagelib, the webpack bundles).
 *   - __readFile(path) / __writeFile(path, str): UTF-8 file IO for the
 *     trace JSON in and the run JSON out.
 *   - hrtime(): monotonic microseconds (wall-time reporting only; the
 *     sim clock is the harness virtual clock, init.js).
 *
 * Exit contract: 0 only if the evaluated script ran to completion AND
 * set globalThis.__replayExit to 0 (the driver's own verdict) AND no
 * unhandled promise rejection occurred. Everything else is nonzero —
 * a dangling promise can never look like success.
 *
 * Build: oracle/qjs/build.sh (QuickJS pinned by commit + source sha256).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "quickjs-libc.h"
#include "sha256.h"
#include "../../port/fdlibm/fdlibm.h"

/* ---- fdlibm Math repoint ---------------------------------------------- */

#define FD_WRAP1(cname, fdfn)                                              \
    static JSValue cname(JSContext *ctx, JSValueConst this_val, int argc, \
                         JSValueConst *argv) {                             \
        double x;                                                          \
        (void)this_val;                                                    \
        if (JS_ToFloat64(ctx, &x, argc >= 1 ? argv[0] : JS_UNDEFINED))     \
            return JS_EXCEPTION;                                           \
        return JS_NewFloat64(ctx, fdfn(x));                                \
    }

#define FD_WRAP2(cname, fdfn)                                              \
    static JSValue cname(JSContext *ctx, JSValueConst this_val, int argc, \
                         JSValueConst *argv) {                             \
        double x, y;                                                       \
        (void)this_val;                                                    \
        if (JS_ToFloat64(ctx, &x, argc >= 1 ? argv[0] : JS_UNDEFINED))     \
            return JS_EXCEPTION;                                           \
        if (JS_ToFloat64(ctx, &y, argc >= 2 ? argv[1] : JS_UNDEFINED))     \
            return JS_EXCEPTION;                                           \
        return JS_NewFloat64(ctx, fdfn(x, y));                             \
    }

FD_WRAP1(js_fd_sin, fd_sin)
FD_WRAP1(js_fd_cos, fd_cos)
FD_WRAP1(js_fd_tan, fd_tan)
FD_WRAP1(js_fd_atan, fd_atan)
FD_WRAP2(js_fd_atan2, fd_atan2)
FD_WRAP2(js_fd_pow, fd_pow)

static void repoint_math(JSContext *ctx) {
    JSValue glob = JS_GetGlobalObject(ctx);
    JSValue math = JS_GetPropertyStr(ctx, glob, "Math");
    JS_SetPropertyStr(ctx, math, "sin",
                      JS_NewCFunction(ctx, js_fd_sin, "sin", 1));
    JS_SetPropertyStr(ctx, math, "cos",
                      JS_NewCFunction(ctx, js_fd_cos, "cos", 1));
    JS_SetPropertyStr(ctx, math, "tan",
                      JS_NewCFunction(ctx, js_fd_tan, "tan", 1));
    JS_SetPropertyStr(ctx, math, "atan",
                      JS_NewCFunction(ctx, js_fd_atan, "atan", 1));
    JS_SetPropertyStr(ctx, math, "atan2",
                      JS_NewCFunction(ctx, js_fd_atan2, "atan2", 2));
    JS_SetPropertyStr(ctx, math, "pow",
                      JS_NewCFunction(ctx, js_fd_pow, "pow", 2));
    JS_FreeValue(ctx, math);
    JS_FreeValue(ctx, glob);
}

/* ---- helpers ------------------------------------------------------------ */

static JSValue js_hrtime(JSContext *ctx, JSValueConst this_val, int argc,
                         JSValueConst *argv) {
    struct timespec ts;
    (void)this_val; (void)argc; (void)argv;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return JS_NewFloat64(ctx, ts.tv_sec * 1e6 + ts.tv_nsec / 1e3);
}

static char *read_file(const char *path, size_t *plen) {
    FILE *f = fopen(path, "rb");
    long sz;
    char *buf;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    buf[sz] = 0;
    fclose(f);
    *plen = (size_t)sz;
    return buf;
}

static JSValue js_read_file(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    const char *path;
    size_t len;
    char *buf;
    JSValue ret;
    (void)this_val;
    if (argc < 1) return JS_ThrowTypeError(ctx, "__readFile: missing path");
    path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    buf = read_file(path, &len);
    if (!buf) {
        JSValue e = JS_ThrowTypeError(ctx, "__readFile: cannot read %s", path);
        JS_FreeCString(ctx, path);
        return e;
    }
    ret = JS_NewStringLen(ctx, buf, len);
    free(buf);
    JS_FreeCString(ctx, path);
    return ret;
}

static JSValue js_eval_file(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    const char *path;
    size_t len;
    char *buf;
    JSValue ret;
    (void)this_val;
    if (argc < 1) return JS_ThrowTypeError(ctx, "__evalFile: missing path");
    path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    buf = read_file(path, &len);
    if (!buf) {
        JSValue e = JS_ThrowTypeError(ctx, "__evalFile: cannot read %s", path);
        JS_FreeCString(ctx, path);
        return e;
    }
    ret = JS_Eval(ctx, buf, len, path, JS_EVAL_TYPE_GLOBAL);
    free(buf);
    JS_FreeCString(ctx, path);
    return ret;
}

static JSValue js_write_file(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    const char *path;
    const char *data;
    size_t len;
    FILE *f;
    size_t wr;
    (void)this_val;
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "__writeFile: need (path, string)");
    path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    data = JS_ToCStringLen(ctx, &len, argv[1]);
    if (!data) { JS_FreeCString(ctx, path); return JS_EXCEPTION; }
    f = fopen(path, "wb");
    if (!f) {
        JSValue e = JS_ThrowTypeError(ctx, "__writeFile: cannot open %s", path);
        JS_FreeCString(ctx, path);
        JS_FreeCString(ctx, data);
        return e;
    }
    wr = fwrite(data, 1, len, f);
    fclose(f);
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, data);
    if (wr != len) return JS_ThrowTypeError(ctx, "__writeFile: short write");
    return JS_UNDEFINED;
}

/* __sha256(typedArray|ArrayBuffer) -> ArrayBuffer(32) */
static JSValue js_sha256(JSContext *ctx, JSValueConst this_val, int argc,
                         JSValueConst *argv) {
    uint8_t *ptr = NULL;
    size_t size = 0;
    uint8_t digest[32];
    (void)this_val;
    if (argc < 1) return JS_ThrowTypeError(ctx, "__sha256: missing data");
    ptr = JS_GetArrayBuffer(ctx, &size, argv[0]);
    if (!ptr) {
        /* not an ArrayBuffer: try a typed-array view */
        size_t byte_offset, byte_length, bpe;
        JSValue ab;
        JS_FreeValue(ctx, JS_GetException(ctx)); /* clear the AB failure */
        ab = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &byte_length,
                                    &bpe);
        if (JS_IsException(ab)) return ab;
        ptr = JS_GetArrayBuffer(ctx, &size, ab);
        JS_FreeValue(ctx, ab);
        if (!ptr) return JS_EXCEPTION;
        ptr += byte_offset;
        size = byte_length;
    }
    sha256(ptr, size, digest);
    return JS_NewArrayBufferCopy(ctx, digest, 32);
}

/* ---- unhandled promise rejections are fatal ---------------------------- */

static int g_had_rejection = 0;

static void on_promise_rejection(JSContext *ctx, JSValueConst promise,
                                 JSValueConst reason, JS_BOOL is_handled,
                                 void *opaque) {
    (void)promise; (void)opaque;
    if (!is_handled) {
        const char *s = JS_ToCString(ctx, reason);
        fprintf(stderr, "qjs-oracle: unhandled promise rejection: %s\n",
                s ? s : "(unprintable)");
        if (s) JS_FreeCString(ctx, s);
        g_had_rejection = 1;
    }
}

/* ---- main --------------------------------------------------------------- */

int main(int argc, char **argv) {
    JSRuntime *rt;
    JSContext *ctx;
    JSValue glob, v;
    char *buf;
    size_t len;
    int ret = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: qjs-oracle script.js [args...]\n");
        return 1;
    }
    if (sha256_self_test() != 0) {
        fprintf(stderr, "qjs-oracle: FATAL: sha256 self-test failed\n");
        return 1;
    }

    rt = JS_NewRuntime();
    /* the 30MB animations bundle + full game state need headroom */
    JS_SetMaxStackSize(rt, 8 * 1024 * 1024);
    ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, argc - 1, argv + 1); /* print, console, scriptArgs */
    js_std_init_handlers(rt);
    JS_SetHostPromiseRejectionTracker(rt, on_promise_rejection, NULL);

    /* BEFORE any JS runs: Math surface = our fdlibm. The env hook exists
     * ONLY so the negative test can prove replay-main.js's boot assertion
     * really detects a missing repoint (a run without the repoint must
     * die at "fdlibm repoint NOT active", never produce a stream). */
    if (getenv("QJS_ORACLE_NO_REPOINT")) {
        fprintf(stderr, "qjs-oracle: WARNING: QJS_ORACLE_NO_REPOINT set — "
                        "Math is HOST libm (negative-test mode)\n");
    } else {
        repoint_math(ctx);
    }

    glob = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, glob, "hrtime",
                      JS_NewCFunction(ctx, js_hrtime, "hrtime", 0));
    JS_SetPropertyStr(ctx, glob, "__readFile",
                      JS_NewCFunction(ctx, js_read_file, "__readFile", 1));
    JS_SetPropertyStr(ctx, glob, "__evalFile",
                      JS_NewCFunction(ctx, js_eval_file, "__evalFile", 1));
    JS_SetPropertyStr(ctx, glob, "__writeFile",
                      JS_NewCFunction(ctx, js_write_file, "__writeFile", 2));
    JS_SetPropertyStr(ctx, glob, "__qjs_sha256",
                      JS_NewCFunction(ctx, js_sha256, "__qjs_sha256", 1));
    JS_FreeValue(ctx, glob);

    buf = read_file(argv[1], &len);
    if (!buf) {
        perror(argv[1]);
        return 1;
    }
    v = JS_Eval(ctx, buf, len, argv[1], JS_EVAL_TYPE_GLOBAL);
    free(buf);
    if (JS_IsException(v)) {
        js_std_dump_error(ctx);
        ret = 1;
    }
    JS_FreeValue(ctx, v);

    /* drain microtasks / pending jobs (the driver is async) */
    js_std_loop(ctx);

    if (ret == 0) {
        /* the driver must have set __replayExit = 0 — completion by proof */
        int32_t code = -1;
        glob = JS_GetGlobalObject(ctx);
        v = JS_GetPropertyStr(ctx, glob, "__replayExit");
        if (JS_IsNumber(v)) JS_ToInt32(ctx, &code, v);
        JS_FreeValue(ctx, v);
        JS_FreeValue(ctx, glob);
        if (code != 0) {
            fprintf(stderr,
                    "qjs-oracle: script did not complete (__replayExit=%d)\n",
                    code);
            ret = 1;
        }
    }
    if (g_had_rejection) {
        fprintf(stderr, "qjs-oracle: failing due to unhandled rejection\n");
        ret = 1;
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return ret;
}
