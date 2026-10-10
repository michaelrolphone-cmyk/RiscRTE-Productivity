#include "ReaderEngine.h"
#include "fontIds.h"
#include <builtinFonts/notoserif_12_regular.h>
#include <builtinFonts/notoserif_12_bold.h>
#include <builtinFonts/notoserif_12_italic.h>
#include <builtinFonts/notoserif_12_bolditalic.h>
#include <builtinFonts/notoserif_14_regular.h>
#include <builtinFonts/notoserif_14_bold.h>
#include <builtinFonts/notoserif_14_italic.h>
#include <builtinFonts/notoserif_14_bolditalic.h>
#include <builtinFonts/notoserif_16_regular.h>
#include <builtinFonts/notoserif_16_bold.h>
#include <builtinFonts/notoserif_16_italic.h>
#include <builtinFonts/notoserif_16_bolditalic.h>
#include <builtinFonts/notoserif_18_regular.h>
#include <builtinFonts/notoserif_18_bold.h>
#include <builtinFonts/notoserif_18_italic.h>
#include <builtinFonts/notoserif_18_bolditalic.h>
#include <builtinFonts/notosans_12_regular.h>
#include <builtinFonts/notosans_12_bold.h>
#include <builtinFonts/notosans_12_italic.h>
#include <builtinFonts/notosans_12_bolditalic.h>
#include <builtinFonts/notosans_14_regular.h>
#include <builtinFonts/notosans_14_bold.h>
#include <builtinFonts/notosans_14_italic.h>
#include <builtinFonts/notosans_14_bolditalic.h>
#include <builtinFonts/notosans_16_regular.h>
#include <builtinFonts/notosans_16_bold.h>
#include <builtinFonts/notosans_16_italic.h>
#include <builtinFonts/notosans_16_bolditalic.h>
#include <builtinFonts/notosans_18_regular.h>
#include <builtinFonts/notosans_18_bold.h>
#include <builtinFonts/notosans_18_italic.h>
#include <builtinFonts/notosans_18_bolditalic.h>
namespace reader {
void installBuiltinFonts(GfxRenderer& renderer){
 static EpdFont f_notoserif_12_regular(&notoserif_12_regular);
 static EpdFont f_notoserif_12_bold(&notoserif_12_bold);
 static EpdFont f_notoserif_12_italic(&notoserif_12_italic);
 static EpdFont f_notoserif_12_bolditalic(&notoserif_12_bolditalic);
 renderer.insertFont(NOTOSERIF_12_FONT_ID,EpdFontFamily(&f_notoserif_12_regular,&f_notoserif_12_bold,&f_notoserif_12_italic,&f_notoserif_12_bolditalic));
 static EpdFont f_notoserif_14_regular(&notoserif_14_regular);
 static EpdFont f_notoserif_14_bold(&notoserif_14_bold);
 static EpdFont f_notoserif_14_italic(&notoserif_14_italic);
 static EpdFont f_notoserif_14_bolditalic(&notoserif_14_bolditalic);
 renderer.insertFont(NOTOSERIF_14_FONT_ID,EpdFontFamily(&f_notoserif_14_regular,&f_notoserif_14_bold,&f_notoserif_14_italic,&f_notoserif_14_bolditalic));
 static EpdFont f_notoserif_16_regular(&notoserif_16_regular);
 static EpdFont f_notoserif_16_bold(&notoserif_16_bold);
 static EpdFont f_notoserif_16_italic(&notoserif_16_italic);
 static EpdFont f_notoserif_16_bolditalic(&notoserif_16_bolditalic);
 renderer.insertFont(NOTOSERIF_16_FONT_ID,EpdFontFamily(&f_notoserif_16_regular,&f_notoserif_16_bold,&f_notoserif_16_italic,&f_notoserif_16_bolditalic));
 static EpdFont f_notoserif_18_regular(&notoserif_18_regular);
 static EpdFont f_notoserif_18_bold(&notoserif_18_bold);
 static EpdFont f_notoserif_18_italic(&notoserif_18_italic);
 static EpdFont f_notoserif_18_bolditalic(&notoserif_18_bolditalic);
 renderer.insertFont(NOTOSERIF_18_FONT_ID,EpdFontFamily(&f_notoserif_18_regular,&f_notoserif_18_bold,&f_notoserif_18_italic,&f_notoserif_18_bolditalic));
 static EpdFont f_notosans_12_regular(&notosans_12_regular);
 static EpdFont f_notosans_12_bold(&notosans_12_bold);
 static EpdFont f_notosans_12_italic(&notosans_12_italic);
 static EpdFont f_notosans_12_bolditalic(&notosans_12_bolditalic);
 renderer.insertFont(NOTOSANS_12_FONT_ID,EpdFontFamily(&f_notosans_12_regular,&f_notosans_12_bold,&f_notosans_12_italic,&f_notosans_12_bolditalic));
 static EpdFont f_notosans_14_regular(&notosans_14_regular);
 static EpdFont f_notosans_14_bold(&notosans_14_bold);
 static EpdFont f_notosans_14_italic(&notosans_14_italic);
 static EpdFont f_notosans_14_bolditalic(&notosans_14_bolditalic);
 renderer.insertFont(NOTOSANS_14_FONT_ID,EpdFontFamily(&f_notosans_14_regular,&f_notosans_14_bold,&f_notosans_14_italic,&f_notosans_14_bolditalic));
 static EpdFont f_notosans_16_regular(&notosans_16_regular);
 static EpdFont f_notosans_16_bold(&notosans_16_bold);
 static EpdFont f_notosans_16_italic(&notosans_16_italic);
 static EpdFont f_notosans_16_bolditalic(&notosans_16_bolditalic);
 renderer.insertFont(NOTOSANS_16_FONT_ID,EpdFontFamily(&f_notosans_16_regular,&f_notosans_16_bold,&f_notosans_16_italic,&f_notosans_16_bolditalic));
 static EpdFont f_notosans_18_regular(&notosans_18_regular);
 static EpdFont f_notosans_18_bold(&notosans_18_bold);
 static EpdFont f_notosans_18_italic(&notosans_18_italic);
 static EpdFont f_notosans_18_bolditalic(&notosans_18_bolditalic);
 renderer.insertFont(NOTOSANS_18_FONT_ID,EpdFontFamily(&f_notosans_18_regular,&f_notosans_18_bold,&f_notosans_18_italic,&f_notosans_18_bolditalic));
}
int builtinFontId(bool sans,unsigned size){switch(size){
case 12:return sans?NOTOSANS_12_FONT_ID:NOTOSERIF_12_FONT_ID;
case 14:return sans?NOTOSANS_14_FONT_ID:NOTOSERIF_14_FONT_ID;
case 16:return sans?NOTOSANS_16_FONT_ID:NOTOSERIF_16_FONT_ID;
case 18:return sans?NOTOSANS_18_FONT_ID:NOTOSERIF_18_FONT_ID;
default:return NOTOSERIF_14_FONT_ID;}}
}
