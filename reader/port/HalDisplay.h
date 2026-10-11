#pragma once
#include "Arduino.h"
#include "ReaderPort.h"
/* Software raster target only. There is no panel/refresh/pin access here. */
class HalDisplay {
 uint8_t* pixels_=nullptr;uint16_t width_=0,height_=0;bool loaned_=false;
public:
 enum RefreshMode{FULL_REFRESH,HALF_REFRESH,FAST_REFRESH};
 enum class GrayscaleMode{Overlay,Absolute};enum class GrayscaleBase{Separate,Combined};
 struct GrayscaleCapabilities{bool asyncBase=false,stripUploads=false;GrayscaleBase base=GrayscaleBase::Separate;};
 static constexpr uint16_t DISPLAY_WIDTH=0,DISPLAY_HEIGHT=0,DISPLAY_WIDTH_BYTES=0;static constexpr uint32_t BUFFER_SIZE=0;
 bool bind(uint8_t* p,uint16_t w,uint16_t h){if(!p||!w||!h||w%8)return false;pixels_=p;width_=w;height_=h;loaned_=false;return true;}
 uint8_t* getFrameBuffer()const{return loaned_?nullptr:pixels_;}
 uint16_t getDisplayWidth()const{return width_;}uint16_t getDisplayHeight()const{return height_;}uint16_t getDisplayWidthBytes()const{return width_/8;}uint32_t getBufferSize()const{return uint32_t(width_/8)*height_;}
 uint8_t* lendFrameBufferStorage(uint32_t* n){if(loaned_)return nullptr;*n=getBufferSize();loaned_=true;return pixels_;}
 void returnFrameBufferStorage(){loaned_=false;clearScreen();}
 void clearScreen(uint8_t color=255)const{if(pixels_&&!loaned_)memset(pixels_,color,getBufferSize());}
 bool isInverted()const{return false;}
 void drawImage(const uint8_t* p,uint16_t x,uint16_t y,uint16_t w,uint16_t h)const{for(unsigned row=0;row<h&&y+row<height_;++row)for(unsigned col=0;col<w&&x+col<width_;++col){uint8_t mask=0x80>>((x+col)%8);auto& b=pixels_[(y+row)*(width_/8)+(x+col)/8];if(p[row*((w+7)/8)+col/8]&(0x80>>(col%8)))b|=mask;else b&=~mask;}}
 GrayscaleCapabilities grayscaleCapabilities(GrayscaleMode=GrayscaleMode::Overlay)const{return {};}
 bool supportsAsyncRefresh()const{return false;}
 void displayBuffer(RefreshMode,bool=false){reader::retain();}void displayBufferAsync(RefreshMode){reader::retain();}void waitRefreshComplete(){reader::retain();}
 void displayGrayscaleBase(RefreshMode,bool){reader::retain();}bool displayGrayscaleBase(GrayscaleMode,RefreshMode,bool){reader::retain();return false;}
 void preconditionGrayscale(){reader::retain();}void preconditionGrayscale(uint16_t,uint16_t,uint16_t,uint16_t){reader::retain();}
 void copyGrayscaleLsbBuffers(const uint8_t*){reader::retain();}void copyGrayscaleMsbBuffers(const uint8_t*){reader::retain();}
 void displayGrayBuffer(bool){reader::retain();}void cleanupGrayscaleBuffers(const uint8_t*){reader::retain();}
 void writeGrayscalePlaneStrip(bool,const uint8_t*,uint16_t,uint16_t){reader::retain();}
};

extern HalDisplay display;
