// facepack.h — installable faces: a JSON file describes the face in layers and
// this engine draws it, with no need to reflash the firmware. Format: docs/FACE_PACKS.md
#pragma once
#include "ui.h"

static const uint8_t THEME_PACK = 200;  // cfg.theme when the active face is a pack

// Loads a pack from JSON in memory (the buffer may be modified while reading).
// On error returns false and a message in err.
bool facePackLoad(char *json, size_t len, char *err, size_t errLen);
#ifdef ESP8266
#include <FS.h>
// Same, reading the file in passes (without loading it all into memory). The file position is reset.
bool facePackLoadFile(File &f, char *err, size_t errLen);
#endif
void facePackUnload();
bool facePackLoaded();
const char *facePackId();
const char *facePackName();
const char *facePackDesc();
const char *facePackAuthor();

// Draws the loaded pack (called by uiRenderClock when s.theme == THEME_PACK).
void facePackRender(const UiState &s, bool full);

// The firmware provides the drawing of JPEG images from LittleFS (honouring uiClip).
// Without this (simulator), the "image" layer becomes a grey rectangle.
extern bool (*uiDrawImage)(const char *path, int x, int y);
