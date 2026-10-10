#pragma once
/* Offscreen book viewport; device geometry and bezel are owned by ui.scene. */
namespace BoardConfig {struct ViewableInsets{int top,right,bottom,left;};
struct Viewport {ViewableInsets viewableInsets;};inline constexpr Viewport ACTIVE{{0,0,0,0}};}
