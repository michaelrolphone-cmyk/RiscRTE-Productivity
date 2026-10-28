#ifndef PRODUCTIVITY_NATIVE_SCROLL_LABELS_H
#define PRODUCTIVITY_NATIVE_SCROLL_LABELS_H
/* Current shared scene uses Orbitron, whose cmap has no ASCII caret.
 * Semantic ASCII words fit native 24/36px controls without a font fallback.
 * Frozen presentations retain their original label bytes. */
#ifdef PORTABLE_NATIVE_SCENE
#define PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL "UP"
#define PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL "DN"
#else
#define PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL "^"
#define PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL "v"
#endif
#endif
