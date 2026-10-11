#pragma once
#include <Epub.h>
#include <Epub/Section.h>
#include <Epub/Page.h>
#include <GfxRenderer.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <SdCardFontRegistry.h>
#include <SdCardFontManager.h>
#include <TtfEpdFont.h>
#include <LibraryIndexFile.h>
#include <LibraryBuilder.h>
#include <array>
namespace reader {
struct Settings {
 char family[96]="Noto Serif";
 uint8_t pointSize=14,margin=15,lineSpacing=1,alignment=0;
 uint8_t paragraphSpacing=0,indent=2,characterSpacing=0,wordSpacing=100;
 uint8_t hyphenation=1,embeddedStyle=1,images=1,orientation=0;
};
struct Position {uint32_t spine=0,offset=0;};
struct Bookmark {Position position;char name[80]{};};
struct BookState {Position position;uint32_t count=0;Bookmark marks[64]{};};
struct StoredSettings {Settings settings;char recentPath[512]{};};
/* New records are separate from the existing Settings/BookState binary ABI. */
struct ReaderUiSettings {uint32_t version=1,scroll=0,invert=0,reserved=0;uint64_t session=0;};
enum { BookStarted=1u, BookFinished=2u, BookHidden=4u };
struct BookSummary {
 uint32_t version=1,flags=0,progress=0,chapter=0,chapters=0,bookmarks=0,remaining=UINT32_MAX;
 uint64_t lastSession=0,identity=0;
 char title[96]{},author[96]{};
};
bool readBookSummary(const std::string&,BookSummary&);

class Engine {
 GfxRenderer renderer;
 FontDecompressor decompressor;
 FontCacheManager fontCache;
 SdCardFontManager sdFonts;
 std::unique_ptr<TtfEpdFont> vectorFont;
 std::array<HalFile,4> vectorFiles;
 std::vector<uint8_t> pixels;
 std::shared_ptr<Epub> book;
 std::unique_ptr<Section> section;
 std::unique_ptr<Page> page;
 int fontId=0,spine=0,pageNumber=0;
 uint32_t targetOffset=0;bool seeking=false;
 std::string anchor;
 uint32_t stateGeneration=0,settingsGeneration=0;
 bool stateWritable=true,settingsWritable=true;
 uint32_t uiGeneration=0,summaryGeneration=0;bool uiWritable=true,summaryWritable=true;
 int scrollOffset=0,seekFraction=-1;
 std::unique_ptr<Section> followingSection;
 std::unique_ptr<Page> followingPage;
 std::vector<uint8_t> previewPixels;
 bool loadUiSettings();bool saveSummary();bool prepareFollowingPage();
 void alignScrollAnchor(uint32_t);
 bool loadSection(int);
 bool selectFont();
 bool saveBook();
 ReaderRenderSpec spec()const;
public:
 StoredSettings preferences;
 ReaderUiSettings ui;BookSummary summary;
 BookState state;
 SdCardFontRegistry fontRegistry;
 library::LibraryIndexFile library;
 std::string message;
 Engine();~Engine();
 bool init(unsigned width,unsigned height);
 bool loadSettings();bool saveSettings();
 bool scanLibrary();bool openLibrary();
 bool open(const std::string& path,bool recent=true);bool close();bool suspend();
 bool step();bool turn(int direction);bool jump(Position);bool jumpToc(unsigned);
 bool setLayout(const Settings&);bool toggleBookmark();bool removeBookmark(unsigned);bool renameBookmark(unsigned,const char*);
 bool render();bool savePosition();
 Position position()const;
 bool waiting()const{return section&&(!page||seeking);}
 bool isOpen()const{return bool(book);}bool busy()const{return section&&(section->isBuilding()||seeking);}
 const uint8_t* bitmap()const{return pixels.data();}size_t bitmapSize()const{return pixels.size();}
 std::string title()const{return book?book->getTitle():"Reader";}
 std::string path()const{return book?book->getPath():"";}
 std::string footer()const;
 bool beginReading(bool restart=false);bool markFinished(bool);bool removeFromLibrary();
 bool setUi(const ReaderUiSettings&);bool jumpProgress(unsigned);bool scrollBy(int pixels);
 unsigned progress()const;unsigned chapter()const{return unsigned(spine)+1;}
 unsigned chapterCount()const{return book?unsigned(book->getSpineItemsCount()):0;}
 unsigned remainingMinutes()const;bool atEnd()const;bool isBookmarked()const;
 std::string author()const{return book?book->getAuthor():"";}
 std::string chapterTitle()const;std::string snippet()const;
 bool renderPreview(unsigned width,unsigned height);
 const uint8_t* previewBitmap()const{return previewPixels.data();}
 size_t previewSize()const{return previewPixels.size();}

 int tocCount()const{return book?book->getTocItemsCount():0;}
 BookMetadataCache::TocEntry toc(unsigned i)const{return book->getTocItem(i);}
 std::vector<uint8_t> fontSizes()const;
};
void installBuiltinFonts(GfxRenderer&);
int builtinFontId(bool sans,unsigned size);
bool readRecord(const std::string&,void*,size_t,uint32_t&,bool&);
bool writeRecord(const std::string&,const void*,size_t,uint32_t&,bool&);
}
