// port/foh/foh_credits_view.h — the credits screen across a lid close.
// See foh_credits_view.c for D63's argument, and foh_viewfile.h for the
// shared file mechanism. Both calls are non-fatal by design: the worst a
// failure costs is a fresh starfield, never a refused resume.
#ifndef FOH_CREDITS_VIEW_H
#define FOH_CREDITS_VIEW_H

#include <stdbool.h>

#include "foh.h"

// Publish the live credits view. Called from the hibernate arm, beside the
// settings save, when the screen being left is the credits.
void foh_credits_view_save(const FohState *s);
// Restore it over a freshly-initialised credits screen, and CONSUME the file.
// False (and a named reason on stderr) means the screen stays as init left
// it — a legitimate outcome, not a failure to report to the player.
bool foh_credits_view_load(FohState *s);

#endif
