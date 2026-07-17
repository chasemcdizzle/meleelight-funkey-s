// port/tools/fk_input.c — uinput button injector for the FunKey-S (M3
// task 5; the ssb64 fk_input pattern, docs/research/funkey-envelope.md
// §"Getting builds onto the device": writing the EXISTING event device
// does NOT inject — you must create your OWN uinput keyboard device;
// its events then flow through the kernel input core into the console/
// SDL path exactly like the physical buttons, which arrive as LETTER
// keysyms: u/d/l/r d-pad, a/b/x/y face, s START, k/n L/R, q MENU).
//
// Static armv7 build via the shared rig build (riglib.sh). Root only
// (the FunKey adb shell is root). Uses the LEGACY uinput_user_dev API
// (this device runs an old kernel; UI_DEV_SETUP needs >= 4.5).
//
//   fk_input <script>
//
// Script grammar (one command per line; '#' comments and blank lines
// ignored; ANY other token is a LOUD parse error, exit 2 — a corrupted
// script must never half-play):
//   d <letter>   key down  (letter a-z)
//   u <letter>   key up
//   s <ms>       sleep milliseconds (CLOCK_MONOTONIC)
//
// The whole script is parsed and validated BEFORE the uinput device is
// created (a syntax error injects nothing). After UI_DEV_CREATE the
// injector settles 300 ms so the kernel/console pick the device up,
// then plays the script, then defensively RELEASES every a-z key
// (idempotent in SDL key state — a stuck key must never outlive the
// injector), destroys the device and exits 0.
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// letter -> Linux KEY_* code (QWERTY key codes, input-event-codes.h)
static const int kLetterKey[26] = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
};

typedef struct {
  char op;   // 'd' down, 'u' up, 's' sleep
  int key;   // KEY_* code for d/u
  long ms;   // sleep duration for s
} Cmd;

static void die(const char *what) {
  perror(what);
  exit(1);
}

static void msleep(long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
  }
}

// The KERNEL's 32-bit-arm input_event layout (16 bytes: 32-bit
// sec/usec + type/code/value). The SDK's musl 1.2 has 64-bit time_t,
// which makes libc's struct input_event 24 bytes — the old FunKey
// kernel consumes 16 of them (one garbled event) and returns a SHORT
// write with errno 0 (measured iter 51). Same class as the iter-38
// "trust no device-libc symbol" rule, extended: any kernel struct
// carrying timestamps must use the KERNEL's ABI layout, never the
// libc's. Timestamps are zero — the input core stamps events itself.
typedef struct {
  uint32_t sec;
  uint32_t usec;
  uint16_t type;
  uint16_t code;
  int32_t value;
} FkKernelEvent;

static void emit(int fd, int type, int code, int value) {
  FkKernelEvent ev;
  memset(&ev, 0, sizeof ev);
  ev.type = (uint16_t)type;
  ev.code = (uint16_t)code;
  ev.value = value;
  if (write(fd, &ev, sizeof ev) != (ssize_t)sizeof ev) {
    die("fk_input: write(input_event)");
  }
}

static void key_event(int fd, int key, int value) {
  emit(fd, EV_KEY, key, value);
  emit(fd, EV_SYN, SYN_REPORT, 0);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: fk_input <script>\n");
    return 2;
  }

  // ---- parse + validate the WHOLE script first --------------------------
  FILE *sf = fopen(argv[1], "r");
  if (!sf) die("fk_input: open script");
  Cmd *cmds = NULL;
  size_t ncmd = 0, cap = 0;
  char line[256];
  int lineno = 0;
  while (fgets(line, sizeof line, sf)) {
    lineno++;
    if (strchr(line, '\n') == NULL && !feof(sf)) {
      fprintf(stderr, "fk_input: script line %d too long\n", lineno);
      return 2;
    }
    // strip comments + whitespace-only lines
    char *hash = strchr(line, '#');
    if (hash) *hash = 0;
    char op = 0;
    char arg[64];
    const int n = sscanf(line, " %c %63s", &op, arg);
    if (n <= 0) continue; // blank
    if (n != 2) {
      fprintf(stderr, "fk_input: script line %d malformed\n", lineno);
      return 2;
    }
    Cmd c;
    memset(&c, 0, sizeof c);
    c.op = op;
    if (op == 'd' || op == 'u') {
      if (strlen(arg) != 1 || arg[0] < 'a' || arg[0] > 'z') {
        fprintf(stderr, "fk_input: script line %d: bad key '%s'\n", lineno,
                arg);
        return 2;
      }
      c.key = kLetterKey[arg[0] - 'a'];
    } else if (op == 's') {
      char *end = NULL;
      c.ms = strtol(arg, &end, 10);
      if (!end || *end != 0 || c.ms < 0 || c.ms > 60000) {
        fprintf(stderr, "fk_input: script line %d: bad sleep '%s'\n",
                lineno, arg);
        return 2;
      }
    } else {
      fprintf(stderr, "fk_input: script line %d: unknown op '%c'\n",
              lineno, op);
      return 2;
    }
    if (ncmd == cap) {
      cap = cap ? cap * 2 : 64;
      cmds = realloc(cmds, cap * sizeof *cmds);
      if (!cmds) die("fk_input: oom");
    }
    cmds[ncmd++] = c;
  }
  fclose(sf);
  if (ncmd == 0) {
    fprintf(stderr, "fk_input: empty script\n");
    return 2;
  }

  // ---- create our OWN uinput keyboard device ----------------------------
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0) fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0) die("fk_input: open /dev/uinput (and /dev/input/uinput)");
  if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) die("fk_input: UI_SET_EVBIT");
  if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) die("fk_input: UI_SET_EVBIT syn");
  for (int i = 0; i < 26; i++) {
    if (ioctl(fd, UI_SET_KEYBIT, kLetterKey[i]) < 0) {
      die("fk_input: UI_SET_KEYBIT");
    }
  }
  struct uinput_user_dev ud;
  memset(&ud, 0, sizeof ud);
  snprintf(ud.name, sizeof ud.name, "mlfk-fk-input");
  ud.id.bustype = BUS_VIRTUAL;
  ud.id.vendor = 0x1;
  ud.id.product = 0x1;
  ud.id.version = 1;
  if (write(fd, &ud, sizeof ud) != (ssize_t)sizeof ud) {
    die("fk_input: write(uinput_user_dev)");
  }
  if (ioctl(fd, UI_DEV_CREATE) < 0) die("fk_input: UI_DEV_CREATE");
  msleep(300); // registration settle before the first event

  // ---- play --------------------------------------------------------------
  for (size_t i = 0; i < ncmd; i++) {
    const Cmd *c = &cmds[i];
    if (c->op == 's') {
      msleep(c->ms);
    } else {
      key_event(fd, c->key, c->op == 'd' ? 1 : 0);
    }
  }

  // defensive release-all (idempotent; a stuck key never outlives us)
  for (int i = 0; i < 26; i++) key_event(fd, kLetterKey[i], 0);
  msleep(100);

  if (ioctl(fd, UI_DEV_DESTROY) < 0) die("fk_input: UI_DEV_DESTROY");
  close(fd);
  free(cmds);
  fprintf(stderr, "fk_input: played %zu commands\n", ncmd);
  return 0;
}
