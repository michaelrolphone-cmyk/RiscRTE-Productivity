#include "ReaderEngine.h"
#include "fontIds.h"
#include <builtinFonts/notoserif_14_regular.h>
#include <builtinFonts/notoserif_14_bold.h>
#include <builtinFonts/notoserif_14_italic.h>
#include <builtinFonts/notoserif_14_bolditalic.h>
namespace reader {
// Small fallback for an SD card without the optional font asset package.
// Full Noto families are supplied as upstream TTF files and streamed from SD.
void installBuiltinFonts(GfxRenderer& renderer){
 static EpdFont regular(&notoserif_14_regular),bold(&notoserif_14_bold);
 static EpdFont italic(&notoserif_14_italic),boldItalic(&notoserif_14_bolditalic);
 renderer.insertFont(NOTOSERIF_14_FONT_ID,EpdFontFamily(&regular,&bold,&italic,&boldItalic));
}
int builtinFontId(bool,unsigned){return NOTOSERIF_14_FONT_ID;}
}
