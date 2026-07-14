/*
 * audiotest.c — FunKey-S SDL 1.2 audio spike (wayfinder ticket #12)
 *
 * First-ever measurement of the FunKey-S SDL 1.2 audio path (ssb64 shipped a
 * silent sink). Modes:
 *
 *   audiotest probe
 *       Try SDL_OpenAudio for {44100,22050} Hz x S16 x {256,512,1024,2048}
 *       samples, stereo. Print desired vs obtained spec, play a 300 ms tone,
 *       report callbacks fired.
 *
 *   audiotest load <freq> <samples> <seconds> <loadpct>
 *       Open at freq/samples, spin a busy thread eating ~loadpct% of the one
 *       core, play a tone for <seconds>. Count late callbacks (inter-callback
 *       interval > 1.25x/1.5x/2x nominal) as an underrun proxy.
 *
 *   audiotest mix <freq> <samples> <seconds> <voices> <music01> <loadpct>
 *       Software-mix <voices> looping 22050 Hz mono voices (fixed-point
 *       phase-accumulator resample to output rate) plus optionally one
 *       pre-decoded stereo music stream (at output rate, from RAM) in the
 *       audio callback. Measure callback thread-CPU time; report avg/max and
 *       cost as % of the 16.67 ms frame budget.
 *
 *   audiotest sd <freq> <samples> <seconds> <path>
 *       Play a tone while a thread loops raw read()s of <path> (e.g. a big
 *       file on the SD card). Report late-callback counts.
 *
 * Output is machine-parseable "KEY=value" lines on stdout.
 */
#define _GNU_SOURCE
#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_VOICES 16
#define VOICE_RATE 22050
#define VOICE_LEN  VOICE_RATE      /* 1 s of mono source per voice */
#define MUSIC_SECONDS 2            /* looped pre-decoded stereo buffer */

static int64_t ts_us(const struct timespec *t) {
    return (int64_t)t->tv_sec * 1000000 + t->tv_nsec / 1000;
}
static int64_t ts_ns(const struct timespec *t) {
    return (int64_t)t->tv_sec * 1000000000LL + t->tv_nsec;
}

struct gstate {
    SDL_AudioSpec obt;
    int mode_mix;                  /* 0 = tone, 1 = mixer */
    /* tone */
    double tone_phase;
    /* mixer */
    int nvoices;
    int16_t *voice[MAX_VOICES];
    uint32_t vphase[MAX_VOICES];   /* 16.16 fixed point into VOICE_LEN */
    uint32_t vstep;                /* 16.16: VOICE_RATE/out_rate */
    int use_music;
    int16_t *music;                /* stereo interleaved at output rate */
    int music_frames;
    int music_pos;
    /* stats */
    volatile int stop;
    int64_t nominal_us;            /* expected callback interval */
    int have_last;
    struct timespec last_mono;
    long cbs;
    long late125, late150, late200;
    int64_t max_interval_us;
    int64_t sum_interval_us;
    int64_t cpu_total_ns, cpu_max_ns;
};
static struct gstate g;

static void fill_tone(int16_t *out, int frames, int ch, int freq) {
    double step = 2.0 * M_PI * 440.0 / freq;
    int i, c;
    for (i = 0; i < frames; i++) {
        int16_t s = (int16_t)(sin(g.tone_phase) * 9000.0);
        g.tone_phase += step;
        if (g.tone_phase > 2.0 * M_PI) g.tone_phase -= 2.0 * M_PI;
        for (c = 0; c < ch; c++) *out++ = s;
    }
}

static void fill_mix(int16_t *out, int frames, int ch) {
    int i, v;
    for (i = 0; i < frames; i++) {
        int32_t accL = 0, accR = 0;
        for (v = 0; v < g.nvoices; v++) {
            int16_t s = g.voice[v][g.vphase[v] >> 16];
            g.vphase[v] += g.vstep;
            if ((g.vphase[v] >> 16) >= VOICE_LEN) g.vphase[v] -= (VOICE_LEN << 16);
            /* per-voice gain ~0.12 in Q8 */
            int32_t sv = ((int32_t)s * 30) >> 8;
            accL += sv;
            accR += sv;
        }
        if (g.use_music) {
            accL += g.music[g.music_pos * 2];
            accR += g.music[g.music_pos * 2 + 1];
            if (++g.music_pos >= g.music_frames) g.music_pos = 0;
        }
        if (accL > 32767) accL = 32767; if (accL < -32768) accL = -32768;
        if (accR > 32767) accR = 32767; if (accR < -32768) accR = -32768;
        if (ch == 2) { *out++ = (int16_t)accL; *out++ = (int16_t)accR; }
        else         { *out++ = (int16_t)((accL + accR) >> 1); }
    }
}

static void audio_cb(void *ud, Uint8 *stream, int len) {
    struct timespec t0, c0, c1;
    (void)ud;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (g.have_last) {
        int64_t dt = ts_us(&t0) - ts_us(&g.last_mono);
        g.sum_interval_us += dt;
        if (dt > g.max_interval_us) g.max_interval_us = dt;
        if (dt > g.nominal_us * 5 / 4) g.late125++;
        if (dt > g.nominal_us * 3 / 2) g.late150++;
        if (dt > g.nominal_us * 2)     g.late200++;
    }
    g.last_mono = t0;
    g.have_last = 1;

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &c0);
    if (g.obt.format == AUDIO_S16LSB || g.obt.format == AUDIO_S16MSB) {
        int frames = len / (2 * g.obt.channels);
        if (g.mode_mix) fill_mix((int16_t *)stream, frames, g.obt.channels);
        else            fill_tone((int16_t *)stream, frames, g.obt.channels, g.obt.freq);
    } else {
        memset(stream, g.obt.silence, len);   /* unexpected format: silence */
    }
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &c1);
    int64_t cpu = ts_ns(&c1) - ts_ns(&c0);
    g.cpu_total_ns += cpu;
    if (cpu > g.cpu_max_ns) g.cpu_max_ns = cpu;
    g.cbs++;
}

/* ---- busy-load thread: pct% duty cycle over a 10 ms period ---- */
static volatile int load_stop = 0;
static volatile unsigned long load_sink = 0;
static void *load_thread(void *arg) {
    int pct = *(int *)arg;
    long busy_us = pct * 100;          /* of a 10000 us period */
    long idle_us = 10000 - busy_us;
    struct timespec t0, t;
    while (!load_stop) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        do {
            unsigned long x = load_sink;
            int k;
            for (k = 0; k < 500; k++) x = x * 1664525UL + 1013904223UL;
            load_sink = x;
            clock_gettime(CLOCK_MONOTONIC, &t);
        } while (ts_us(&t) - ts_us(&t0) < busy_us);
        if (idle_us > 0) usleep(idle_us);
    }
    return NULL;
}

/* ---- SD reader thread: loop raw reads of a file ----
 * O_DIRECT (aligned) so every pass truly hits the SD card; without it the
 * file lands in page cache after the first pass and we measure RAM reads.
 * Falls back to fadvise(DONTNEED) per pass if O_DIRECT is refused. */
static volatile int sd_stop = 0;
static int64_t sd_bytes_read = 0;
static int sd_direct = 0;
static const char *sd_path;
static void *sd_thread(void *arg) {
    char *buf;
    (void)arg;
    if (posix_memalign((void **)&buf, 4096, 256 * 1024)) return NULL;
    while (!sd_stop) {
        int fd = open(sd_path, O_RDONLY | O_DIRECT);
        if (fd >= 0) sd_direct = 1;
        else fd = open(sd_path, O_RDONLY);
        if (fd < 0) { fprintf(stderr, "sd: open %s failed\n", sd_path); free(buf); return NULL; }
        ssize_t n;
        while (!sd_stop && (n = read(fd, buf, 256 * 1024)) > 0) sd_bytes_read += n;
        if (!sd_direct) posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        close(fd);
    }
    free(buf);
    return NULL;
}

static int open_audio(int freq, int samples, SDL_AudioSpec *obt) {
    SDL_AudioSpec want;
    memset(&want, 0, sizeof want);
    want.freq = freq;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = samples;
    want.callback = audio_cb;
    memset(obt, 0, sizeof *obt);
    return SDL_OpenAudio(&want, obt);
}

static void reset_stats(void) {
    g.have_last = 0; g.cbs = 0;
    g.late125 = g.late150 = g.late200 = 0;
    g.max_interval_us = 0; g.sum_interval_us = 0;
    g.cpu_total_ns = 0; g.cpu_max_ns = 0;
    g.tone_phase = 0;
}

static void setup_mixer(int nvoices, int use_music) {
    int v, i;
    g.mode_mix = 1;
    g.nvoices = nvoices;
    g.use_music = use_music;
    g.vstep = (uint32_t)(((uint64_t)VOICE_RATE << 16) / g.obt.freq);
    for (v = 0; v < nvoices; v++) {
        g.voice[v] = malloc(VOICE_LEN * sizeof(int16_t));
        uint32_t seed = 12345u * (v + 1);
        double ph = 0, st = 2.0 * M_PI * (200.0 + 90.0 * v) / VOICE_RATE;
        for (i = 0; i < VOICE_LEN; i++) {
            seed = seed * 1664525u + 1013904223u;
            int16_t noise = (int16_t)((seed >> 16) & 0x3fff) - 8192;
            g.voice[v][i] = (int16_t)(sin(ph) * 12000.0 * 0.5 + noise * 0.5);
            ph += st;
        }
        g.vphase[v] = ((uint32_t)(v * 1234) % VOICE_LEN) << 16;
    }
    g.music_frames = g.obt.freq * MUSIC_SECONDS;
    g.music = malloc((size_t)g.music_frames * 2 * sizeof(int16_t));
    double ph = 0;
    for (i = 0; i < g.music_frames; i++) {
        double f = 110.0 + 60.0 * sin(2.0 * M_PI * i / (double)g.music_frames);
        ph += 2.0 * M_PI * f / g.obt.freq;
        int16_t s = (int16_t)(sin(ph) * 8000.0);
        g.music[i * 2] = s;
        g.music[i * 2 + 1] = (int16_t)(s / 2);
    }
    g.music_pos = 0;
}

static void print_spec(const char *tag, const SDL_AudioSpec *s) {
    printf("%s freq=%d format=0x%04x channels=%d samples=%d size=%u silence=%d\n",
           tag, s->freq, s->format, s->channels, s->samples,
           (unsigned)s->size, s->silence);
}

static void report(const char *tag, int seconds) {
    double expected = (double)g.obt.freq / g.obt.samples * seconds;
    double avg_int = g.cbs > 1 ? (double)g.sum_interval_us / (g.cbs - 1) : 0;
    double cpu_avg_us = g.cbs ? (double)g.cpu_total_ns / g.cbs / 1000.0 : 0;
    double cps = (double)g.obt.freq / g.obt.samples;
    double cpu_per_sec_ms = cpu_avg_us * cps / 1000.0;
    double frame_ms = cpu_per_sec_ms / 60.0;           /* audio CPU per 60fps frame */
    double frame_pct = frame_ms / 16.667 * 100.0;
    printf("%s cbs=%ld expected=%.0f nominal_us=%lld avg_interval_us=%.1f "
           "max_interval_us=%lld late125=%ld late150=%ld late200=%ld "
           "cb_cpu_avg_us=%.1f cb_cpu_max_us=%.1f cpu_ms_per_frame=%.4f "
           "frame_budget_pct=%.2f\n",
           tag, g.cbs, expected, (long long)g.nominal_us, avg_int,
           (long long)g.max_interval_us, g.late125, g.late150, g.late200,
           cpu_avg_us, g.cpu_max_ns / 1000.0, frame_ms, frame_pct);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: audiotest probe|load|mix|sd ...\n"); return 2; }
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        printf("SDL_INIT_AUDIO_FAIL err=%s\n", SDL_GetError());
        return 1;
    }
    char drv[64] = "?";
    if (SDL_AudioDriverName(drv, sizeof drv)) printf("DRIVER=%s\n", drv);
    else printf("DRIVER=unknown\n");

    if (!strcmp(argv[1], "probe")) {
        int freqs[] = { 44100, 22050 };
        int bufs[] = { 256, 512, 1024, 2048 };
        int fi, bi;
        for (fi = 0; fi < 2; fi++) for (bi = 0; bi < 4; bi++) {
            memset(&g, 0, sizeof g);
            printf("PROBE req_freq=%d req_samples=%d\n", freqs[fi], bufs[bi]);
            if (open_audio(freqs[fi], bufs[bi], &g.obt) != 0) {
                printf("  OPEN_FAIL err=%s\n", SDL_GetError());
                continue;
            }
            print_spec("  OBTAINED", &g.obt);
            g.nominal_us = (int64_t)g.obt.samples * 1000000 / g.obt.freq;
            reset_stats();
            SDL_PauseAudio(0);
            SDL_Delay(300);
            SDL_PauseAudio(1);
            printf("  RAN cbs=%ld (300ms, nominal_us=%lld)\n", g.cbs,
                   (long long)g.nominal_us);
            SDL_CloseAudio();
        }
        SDL_Quit();
        return 0;
    }

    if (argc >= 5 && (!strcmp(argv[1], "load") || !strcmp(argv[1], "mix") ||
                      !strcmp(argv[1], "sd"))) {
        int freq = atoi(argv[2]);
        int samples = atoi(argv[3]);
        int seconds = atoi(argv[4]);
        memset(&g, 0, sizeof g);
        if (open_audio(freq, samples, &g.obt) != 0) {
            printf("OPEN_FAIL err=%s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        print_spec("OBTAINED", &g.obt);
        g.nominal_us = (int64_t)g.obt.samples * 1000000 / g.obt.freq;

        pthread_t lt, st;
        int loadpct = 0, have_load = 0, have_sd = 0;

        if (!strcmp(argv[1], "load")) {
            loadpct = argc > 5 ? atoi(argv[5]) : 80;
        } else if (!strcmp(argv[1], "mix")) {
            int nvoices = argc > 5 ? atoi(argv[5]) : 8;
            int music = argc > 6 ? atoi(argv[6]) : 1;
            loadpct = argc > 7 ? atoi(argv[7]) : 0;
            if (nvoices > MAX_VOICES) nvoices = MAX_VOICES;
            setup_mixer(nvoices, music);
            printf("MIX voices=%d music=%d load=%d%%\n", nvoices, music, loadpct);
        } else { /* sd */
            sd_path = argv[5];
            loadpct = argc > 6 ? atoi(argv[6]) : 0;
            printf("SDREAD path=%s load=%d%%\n", sd_path, loadpct);
            have_sd = 1;
        }

        if (loadpct > 0) {
            have_load = 1;
            pthread_create(&lt, NULL, load_thread, &loadpct);
            printf("LOAD pct=%d\n", loadpct);
        }

        reset_stats();
        SDL_PauseAudio(0);
        SDL_Delay(500);            /* let pipeline settle */
        reset_stats();             /* measure steady state only */
        if (have_sd) pthread_create(&st, NULL, sd_thread, NULL);
        SDL_Delay(seconds * 1000);
        if (have_sd) { sd_stop = 1; pthread_join(st, NULL); }
        SDL_PauseAudio(1);
        if (have_load) { load_stop = 1; pthread_join(lt, NULL); }
        report("RESULT", seconds);
        if (have_sd) printf("SD_BYTES=%lld direct=%d (%.1f MB/s)\n",
                            (long long)sd_bytes_read, sd_direct,
                            sd_bytes_read / 1048576.0 / seconds);
        SDL_CloseAudio();
        SDL_Quit();
        return 0;
    }

    fprintf(stderr, "bad args\n");
    SDL_Quit();
    return 2;
}
