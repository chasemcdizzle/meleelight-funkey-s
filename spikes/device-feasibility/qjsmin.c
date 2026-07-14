/*
 * qjsmin — minimal QuickJS embedder for the FunKey-S feasibility spike
 * (ticket #8, Experiment 2). Evaluates one JS file with std/os helpers,
 * no REPL, no qjsc, so it cross-compiles in a single gcc invocation:
 *
 *   arm-funkey-linux-musleabihf-gcc -O2 -static -D_GNU_SOURCE \
 *     -DCONFIG_VERSION='"spike"' -I quickjs \
 *     quickjs/quickjs.c quickjs/cutils.c quickjs/libregexp.c \
 *     quickjs/libunicode.c quickjs/dtoa.c quickjs/quickjs-libc.c qjsmin.c \
 *     -o qjsmin -lm -lpthread
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "quickjs-libc.h"

/* hrtime(): monotonic microseconds, exposed as a JS global */
static JSValue js_hrtime(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return JS_NewFloat64(ctx, ts.tv_sec * 1e6 + ts.tv_nsec / 1e3);
}

static void print_rss(void) {
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    if (!f) return;
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, "VmRSS", 5) || !strncmp(line, "VmHWM", 5))
            fprintf(stderr, "%s", line);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: qjsmin file.js [args]\n"); return 1; }
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, argc - 1, argv + 1);
    js_std_init_handlers(rt);
    JSValue glob = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, glob, "hrtime",
                      JS_NewCFunction(ctx, js_hrtime, "hrtime", 0));
    JS_FreeValue(ctx, glob);

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (fread(buf, 1, sz, f) != (size_t)sz) { fprintf(stderr, "short read\n"); return 1; }
    buf[sz] = 0;
    fclose(f);

    JSValue v = JS_Eval(ctx, buf, sz, argv[1], JS_EVAL_TYPE_GLOBAL);
    int ret = 0;
    if (JS_IsException(v)) { js_std_dump_error(ctx); ret = 1; }
    JS_FreeValue(ctx, v);
    js_std_loop(ctx);
    print_rss();
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return ret;
}
