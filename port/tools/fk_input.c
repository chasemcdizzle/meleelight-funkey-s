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
// Script grammar — ANCHORED FULL-LINE WHITELIST (PROCESS §3, iter 53;
// review-51 M1: the old `sscanf(" %c %63s")` ignored trailing tokens,
// so an accidentally joined line like `d l s 250` half-played as
// `d l`). Measured from the committed s1-session.script corpus (no CR,
// trailing newline, single spaces, full-line comments only). Every
// line must be EXACTLY one of:
//   ""             empty
//   "#..."         comment ('#' at column 0; NO inline comments)
//   "d <a-z>"      key down  (length 3: op, ONE space, one letter)
//   "u <a-z>"      key up
//   "s <digits>"   sleep ms  (1-5 digits, value 0..60000)
// Anything that merely resembles a command — trailing junk, extra
// spaces, a CR byte, a final line without its newline (truncated
// write) — is corruption: LOUD parse error, exit 2, BEFORE any
// injection. A corrupted script must never half-play.
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
    size_t len = strlen(line);
    if (len == 0 || line[len - 1] != '\n') {
      // no newline: either the line overflows the buffer or the file's
      // final line lost its newline (truncated write) — both corruption
      fprintf(stderr,
              "fk_input: script line %d %s — corrupt script\n", lineno,
              feof(sf) ? "missing its trailing newline" : "too long");
      return 2;
    }
    line[--len] = 0;
    // anchored full-line whitelist grammar (header comment; measured
    // from the committed script corpus — zero false rejections proven)
    if (len == 0) continue; // empty line
    if (memchr(line, '\r', len)) { // CRLF/CR corruption (comments too)
      fprintf(stderr, "fk_input: script line %d carries a CR byte\n",
              lineno);
      return 2;
    }
    if (line[0] == '#') continue; // full-line comment (column 0 only)
    Cmd c;
    memset(&c, 0, sizeof c);
    c.op = line[0];
    if (line[0] == 'd' || line[0] == 'u') {
      // EXACTLY "d <a-z>" / "u <a-z>": length 3, one space, one letter
      if (len != 3 || line[1] != ' ' || line[2] < 'a' || line[2] > 'z') {
        fprintf(stderr, "fk_input: script line %d malformed ('%s')\n",
                lineno, line);
        return 2;
      }
      c.key = kLetterKey[line[2] - 'a'];
    } else if (line[0] == 's') {
      // EXACTLY "s <1-5 digits>", value 0..60000
      if (len < 3 || len > 7 || line[1] != ' ') {
        fprintf(stderr, "fk_input: script line %d malformed ('%s')\n",
                lineno, line);
        return 2;
      }
      long v = 0;
      for (size_t i = 2; i < len; i++) {
        if (line[i] < '0' || line[i] > '9') {
          fprintf(stderr, "fk_input: script line %d: bad sleep ('%s')\n",
                  lineno, line);
          return 2;
        }
        v = v * 10 + (line[i] - '0');
      }
      if (v > 60000) {
        fprintf(stderr, "fk_input: script line %d: sleep %ld > 60000 ms\n",
                lineno, v);
        return 2;
      }
      c.ms = v;
    } else {
      fprintf(stderr, "fk_input: script line %d: unknown op ('%s')\n",
              lineno, line);
      return 2;
    }
    if (ncmd == cap) {
      cap = cap ? cap * 2 : 64;
      cmds = realloc(cmds, cap * sizeof *cmds);
      if (!cmds) die("fk_input: oom");
    }
    cmds[ncmd++] = c;
  }
  if (ferror(sf)) {
    // review-51 M1: a mid-file read error must never let a valid PREFIX
    // play as the whole script
    fprintf(stderr, "fk_input: read error on the script — refusing to play a prefix\n");
    return 2;
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
