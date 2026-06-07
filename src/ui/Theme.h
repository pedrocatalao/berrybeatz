#pragma once
#include "imgui.h"

// ─────────────────────────────────────────────────────────────────────────────
// BerryBeatz visual theme — single source of truth for colours, style
// constants, and fonts. All values are compile-time constants except the font
// pointers, which are assigned in main.cpp once ImGui fonts are built.
// ─────────────────────────────────────────────────────────────────────────────


// ── Fonts ─────────────────────────────────────────────────────────────────────
inline ImFont* g_fontBtnTab    = nullptr;   // buttons, tabs, fields     — Michroma 13
inline ImFont* g_fontKnobLabel = nullptr;   // knob labels               — Michroma 13
inline ImFont* g_fontKnobValue = nullptr;   // knob value text           — 35px2 8
inline ImFont* g_fontRowNum    = nullptr;   // tracker row number column — 35px2 11
inline ImFont* g_fontTracker   = nullptr;   // tracker pattern cells     — terminal-6-mono 11
inline ImFont* g_fontOrderRow  = nullptr;   // pattern order row         — 35px2 10
inline ImFont* g_fontSidebar   = nullptr;   // sidebar                   — 35px2 9

// Legacy aliases — existing code uses these; map to semantic globals above
#define g_fontUI   g_fontBtnTab
#define g_fontMono g_fontTracker


// ── Background surfaces ───────────────────────────────────────────────────────
// Dark navy spectrum — each level is ~10 units brighter than the one below.
static constexpr ImU32 kClrBgDeep    = IM_COL32(  0,   0,  20, 255);  // deepest: toolbar, popup bg
static constexpr ImU32 kClrBgBase    = IM_COL32( 10,  10,  22, 255);  // base surface: tabs, knob body
static constexpr ImU32 kClrBgPanel   = IM_COL32( 20,  20,  40, 255);  // panels: tab bar, sidebar, beat rows
static constexpr ImU32 kClrBgContent = IM_COL32( 20,  20,  32, 255);  // main content area
static constexpr ImU32 kClrBgRaised  = IM_COL32( 20,  20,  33, 255);  // pattern cells normal bg
static constexpr ImU32 kClrBgWidget  = IM_COL32( 14,  14,  26, 255);  // editor/panel backgrounds
static constexpr ImU32 kClrBgDark    = IM_COL32( 10,  10,  18, 255);  // waveform display
static constexpr ImU32 kClrBgOverlay = IM_COL32( 30,  30,  60, 255);  // active channel header
static constexpr ImU32 kClrBgMuted   = IM_COL32( 20,  20,  30, 255);  // muted channel header
static constexpr ImU32 kClrBgPlugin  = IM_COL32( 20,  20,  35, 255);  // plugin row
static constexpr ImU32 kClrBgTitle   = IM_COL32(  8,   8,  30, 255);  // settings modal title bar


// ── Text ──────────────────────────────────────────────────────────────────────
static constexpr ImU32  kClrTextBright  = IM_COL32(208, 208, 224, 255);  // primary / active tab
static constexpr ImU32  kClrTextDim     = IM_COL32(110, 110, 130, 255);  // inactive tab
static constexpr ImU32  kClrTextLabel   = IM_COL32(160, 160, 180, 255);  // knob labels, close btn icon
static constexpr ImU32  kClrTextHint    = IM_COL32(128, 128, 166, 255);  // subtle navigation/hint labels
static constexpr ImVec4 kClrTextHintV4  = { 0.5f, 0.5f, 0.65f, 1.f };
static constexpr ImU32 kClrTextValue   = IM_COL32(208, 208,  80, 255);  // knob/edit values (yellow)
static constexpr ImU32 kClrTextChannel = IM_COL32(200, 200, 120, 255);  // active channel name
static constexpr ImU32 kClrTextMuted   = IM_COL32( 60,  60,  60, 255);  // muted channel name
static constexpr ImU32 kClrTextSymbol  = IM_COL32(210, 210, 220, 255);  // window control symbols
static constexpr ImU32 kClrTextTitle   = IM_COL32(200, 200, 220, 255);  // settings modal title


// ── Accent colours ────────────────────────────────────────────────────────────
static constexpr ImU32 kClrCyan       = IM_COL32(  0, 220, 255, 255);  // glow, knob arc, waveform
static constexpr ImU32 kClrCyanDim    = IM_COL32(  0, 180, 210, 100);  // knob arc body fill
static constexpr ImU32 kClrLedGreen   = IM_COL32(100, 220, 160, 255);  // default button LED
static constexpr ImU32 kClrBlue       = IM_COL32( 40,  85, 145, 255);  // current row, order selection
static constexpr ImU32 kClrRed        = IM_COL32(160,  30,  30, 255);  // record / danger button
static constexpr ImU32 kClrRedHover   = IM_COL32(190,  50,  50, 255);  // record / danger hovered
static constexpr ImU32 kClrOrangeHint = IM_COL32(255, 153,  76, 255);  // "move a knob" MIDI hint (ImVec4 1,0.6,0.3)


// ── LCD widgets ───────────────────────────────────────────────────────────────
static constexpr ImU32 kClrLcdBg   = IM_COL32( 10,  30,  40, 255);  // LCD display background
static constexpr ImU32 kClrLcdText = IM_COL32(100, 220, 160, 255);  // LCD green text
// Returns kClrLcdText with a custom alpha (0–255).
inline ImU32 LcdTextA(uint8_t a) { return IM_COL32(100, 220, 160, a); }


// ── Tab bar ───────────────────────────────────────────────────────────────────
static constexpr ImU32   kClrTabBg        = IM_COL32( 10,  10,  22, 255);
static constexpr ImU32   kClrTabHover     = IM_COL32( 35,  35,  60, 255);
static constexpr ImU32   kClrTabBodyBg    = IM_COL32( 15,  15,  20, 255);
static constexpr ImVec2  kTabFramePad     = {8.5f, 4.f};
static constexpr ImVec2  kTabInnerSpacing = {1.f, 0.f};


// ── Toolbar ───────────────────────────────────────────────────────────────────
static constexpr ImU32  kClrToolbarGradTop    = IM_COL32(  0,   0,  20, 255);
static constexpr ImU32  kClrToolbarGradBot    = IM_COL32( 20,  20,  40, 255);
static constexpr ImU32  kClrStatusGradTop     = IM_COL32( 20,  20,  40, 255);
static constexpr ImU32  kClrStatusGradBot     = IM_COL32( 15,   15, 35, 255);
static constexpr ImU32  kClrBtnBg             = IM_COL32( 45,  45,  68, 255);
static constexpr ImU32  kClrBtnHover          = IM_COL32( 65,  65,  98, 255);
static constexpr ImU32  kClrBtnActive         = IM_COL32( 80,  80, 120, 255);
static constexpr ImU32  kClrBtnBorder         = IM_COL32( 80,  80, 112, 255);
static constexpr ImU32  kClrKnobBoxBg         = IM_COL32( 28,  28,  44,  86);
static constexpr ImVec2 kToolbarFramePad      = {8.f, 2.f};
static constexpr ImVec2 kToolbarItemSpacing   = {3.f, 0.f};
static constexpr ImVec2 kToolbarWindowPad     = {8.f, 0.f};


// ── Window control buttons ────────────────────────────────────────────────────
static constexpr ImU32 kClrWinCtrlBg    = IM_COL32( 38,  38,  58, 255);
static constexpr ImU32 kClrWinClose     = IM_COL32(140,  28,  28, 255);
static constexpr ImU32 kClrWinMinimize  = IM_COL32(120,  95,  20, 255);
static constexpr ImU32 kClrWinMaximize  = IM_COL32( 22,  68, 120, 255);
static constexpr ImU32 kClrWinFullscr   = IM_COL32( 18, 110,  75, 255);


// ── Scrollbar ─────────────────────────────────────────────────────────────────
static constexpr ImU32 kClrScrollBg     = IM_COL32(  0,   0,   0,   0);
static constexpr ImU32 kClrScrollGrab   = IM_COL32( 40,  85, 145, 200);
static constexpr ImU32 kClrScrollHover  = IM_COL32( 55, 112, 175, 230);
static constexpr ImU32 kClrScrollActive = IM_COL32( 66, 130, 205, 255);


// ── Pattern editor cell colours ───────────────────────────────────────────────
static constexpr ImVec4 kCellBgNormal   = {20/255.f,  20/255.f,  33/255.f, 1.f};
static constexpr ImVec4 kCellBgBeat     = {46/255.f,  46/255.f,  90/255.f, 0.3f};
static constexpr ImVec4 kCellBgMeasure  = {46/255.f,  46/255.f,  90/255.f, 0.4f};
static constexpr ImVec4 kCellTextNormal = {224/255.f, 224/255.f,  64/255.f, 1.f};
static constexpr ImVec4 kCellTextCurRow = {240/255.f, 240/255.f,  80/255.f, 1.f};
static constexpr ImVec4 kCellNote       = {100/255.f, 210/255.f, 220/255.f, 1.f};  // soft cyan
static constexpr ImVec4 kCellInstr      = {220/255.f, 200/255.f, 110/255.f, 1.f};  // soft yellow
static constexpr ImVec4 kCellVolume     = {250/255.f, 100/255.f, 140/255.f, 1.f};  // soft pink
static constexpr ImVec4 kCellEffect     = {120/255.f, 200/255.f, 130/255.f, 1.f};  // soft green
static constexpr ImVec4 kCellDim        = { 80/255.f,  80/255.f,  80/255.f, 1.f};  // empty dots
static constexpr ImVec4 kCellChanHdr    = { 30/255.f,  30/255.f,  60/255.f, 1.f};  // channel header
static constexpr ImVec4 kCellOrderBg    = {  0/255.f,   0/255.f,   0/255.f, 0.f};  // order list (transparent)
static constexpr ImVec4 kCellOrderCur   = { 64/255.f,  64/255.f, 160/255.f, 1.f};  // order list current

static constexpr ImU32 kClrPatToolbarBg  = IM_COL32( 10,  10,  20, 255);
static constexpr ImU32 kClrPatSeparator  = IM_COL32( 46,  46,  90, 220);
static constexpr ImU32 kClrPatDivider    = IM_COL32( 46,  46,  92, 200);
static constexpr ImU32 kClrPatSelection  = IM_COL32( 80,  80, 200,  60);
static constexpr ImU32 kClrPatEditCell   = IM_COL32(255, 220,  80,  80);
static constexpr ImU32 kClrPatEditBorder = IM_COL32(255, 255, 255, 220);
static constexpr ImU32 kClrPatRowBg      = IM_COL32( 21,  21,  30, 255);
static constexpr ImU32 kClrPatCurRowBg   = IM_COL32( 40,  85, 145, 145);
static constexpr ImU32 kClrPatWaveform   = IM_COL32(190, 190, 200, 220);
static constexpr ImU32 kClrPatWaveMuted  = IM_COL32( 80,  80,  80, 120);
static constexpr ImU32 kClrWaveGradBase  = IM_COL32( 14,  15,  24, 255);  // solid base fill
static constexpr ImU32 kClrWaveGradTL    = IM_COL32( 58,  54,  72, 255);  // top-left  — brightest
static constexpr ImU32 kClrWaveGradTR    = IM_COL32( 20,  22,  30, 255);  // top-right
static constexpr ImU32 kClrWaveGradBR    = IM_COL32( 18,  20,  32, 255);  // bot-right — darkest
static constexpr ImU32 kClrWaveGradBL    = IM_COL32( 18,  20,  32, 255);  // bot-left

static constexpr ImU32 kClrToggleBg      = IM_COL32( 64,  64, 160, 255);
static constexpr ImU32 kClrToggleBgHover = IM_COL32( 80,  80, 190, 255);


// ── VU meter ──────────────────────────────────────────────────────────────────
static constexpr float kVuThreshHigh = 0.80f;  // above → red/purple zone
static constexpr float kVuThreshMid  = 0.60f;  // above → pink zone
static constexpr float kVuBarW       = 2.f;
static constexpr float kVuStride     = 3.f;

// Lit segment colours (alpha 225 = slightly translucent for additive blending)
static constexpr ImU32 kVuCyanLit    = IM_COL32(  0, 220, 255, 225);
static constexpr ImU32 kVuCyanUnlit  = IM_COL32(  0,  35,  45, 225);
static constexpr ImU32 kVuPinkLit    = IM_COL32(255,  30, 180, 225);
static constexpr ImU32 kVuPinkUnlit  = IM_COL32( 45,   0,  30, 225);
// Toolbar VU high zone (red) / TrackerView high zone (violet) differ intentionally
static constexpr ImU32 kVuRedLit     = IM_COL32(255,   0,  90, 225);  // toolbar
static constexpr ImU32 kVuRedUnlit   = IM_COL32( 45,   0,  20, 225);
static constexpr ImU32 kVuVioletLit  = IM_COL32(200,   0, 255,  90);  // tracker (lower alpha)
static constexpr ImU32 kVuVioletUnlit= IM_COL32( 45,   0,  20, 225);


// ── Knob ──────────────────────────────────────────────────────────────────────
static constexpr float  kKnobSize      = 36.f;
static constexpr ImU32  kClrKnobBg     = IM_COL32( 14,  14,  34, 255);
static constexpr ImU32  kClrKnobBorder = IM_COL32( 64,  64, 114, 255);
static constexpr ImU32  kClrKnobTrack  = IM_COL32( 46,  46,  90, 255);
static constexpr ImU32  kClrKnobArc    = IM_COL32(  0, 180, 210, 100);  // lit arc body
static constexpr ImU32  kClrKnobTick   = IM_COL32(120, 120, 180, 200);  // bipolar centre tick


// ── Envelope graph ────────────────────────────────────────────────────────────
static constexpr ImU32 kClrEnvBg       = IM_COL32( 10,  10,  22, 255);
static constexpr ImU32 kClrEnvGrid     = IM_COL32( 40,  40,  70, 255);
static constexpr ImU32 kClrEnvLine     = IM_COL32(  0, 200, 255, 255);
static constexpr ImU32 kClrEnvNode     = IM_COL32(  0, 200, 255, 255);
static constexpr ImU32 kClrEnvNodeHov  = IM_COL32(255, 220,   0, 255);  // yellow on hover
static constexpr ImU32 kClrEnvMidLine  = IM_COL32( 80,  80, 120, 180);
static constexpr ImU32 kClrEnvLoopRgn  = IM_COL32(  0,  80, 200,  60);
static constexpr ImU32 kClrEnvLoopLine = IM_COL32(  0, 100, 255, 200);
static constexpr ImU32 kClrEnvSusRgn   = IM_COL32(  0, 160,  80,  60);
static constexpr ImU32 kClrEnvSusLine  = IM_COL32(  0, 200, 100, 200);
static constexpr ImU32 kClrEnvBorder   = IM_COL32( 60,  60, 100, 255);
static constexpr ImU32 kClrEnvNodeBrd  = IM_COL32(  0,   0,   0, 180);
static constexpr float kEnvNodeRadius  = 5.f;


// ── Waveform display ──────────────────────────────────────────────────────────
static constexpr ImU32 kClrWaveBg      = IM_COL32( 10,  10,  18, 255);
static constexpr ImU32 kClrWaveZero    = IM_COL32( 40,  40,  60, 255);
static constexpr ImU32 kClrWaveBar     = IM_COL32(  0, 190, 255, 200);
static constexpr ImU32 kClrWaveBorder  = IM_COL32( 40,  40,  70, 255);
static constexpr ImU32 kClrWaveLoopRgn = IM_COL32(  0,  80, 180,  50);
static constexpr ImU32 kClrWaveLoopLn  = IM_COL32(  0, 120, 255, 200);
static constexpr ImU32 kClrWaveSusRgn  = IM_COL32(  0, 150,  80,  50);
static constexpr ImU32 kClrWaveSusLn   = IM_COL32(  0, 200, 100, 200);


// ── Keyboard (instrument / sample views) ─────────────────────────────────────
static constexpr float kKeyBlackWidthRatio  = 0.65f;
static constexpr float kKeyBlackHeightRatio = 0.60f;
static constexpr float kKeyBlackXOffset     = 7.f;

static constexpr ImU32 kClrKeyWhiteActive   = IM_COL32(180, 220, 255, 255);
static constexpr ImU32 kClrKeyWhiteHover    = IM_COL32(210, 230, 255, 255);
static constexpr ImU32 kClrKeyWhiteNormal   = IM_COL32(230, 230, 240, 255);
static constexpr ImU32 kClrKeyWhiteUnassign = IM_COL32(185, 185, 192, 255);
static constexpr ImU32 kClrKeyBorder        = IM_COL32( 80,  80, 100, 255);
static constexpr ImU32 kClrKeyMiddleC       = IM_COL32(  0, 160, 255, 200);
static constexpr ImU32 kClrKeyLabel         = IM_COL32( 80,  80, 110, 200);

static constexpr ImU32 kClrKeyBlackActive   = IM_COL32( 80, 120, 180, 255);
static constexpr ImU32 kClrKeyBlackHover    = IM_COL32( 60,  80, 120, 255);
static constexpr ImU32 kClrKeyBlackNormal   = IM_COL32( 30,  30,  40, 255);
static constexpr ImU32 kClrKeyBlackUnassign = IM_COL32( 50,  50,  58, 255);
static constexpr ImU32 kClrKeyBlackBorder   = IM_COL32(  0,   0,   0, 255);
static constexpr ImU32 kClrKeyBlackLabel    = IM_COL32(180, 180, 210, 200);


// ── Settings modal ────────────────────────────────────────────────────────────
static constexpr ImU32 kClrModalBg      = IM_COL32(  0,   0,  20, 255);
static constexpr ImU32 kClrModalGradTop = IM_COL32(  0,   0,  20, 100);
static constexpr ImU32 kClrModalGradBot = IM_COL32( 20,  20,  40, 100);
static constexpr ImU32 kClrModalClose   = IM_COL32(160,  30,  30, 200);  // hover
static constexpr ImU32 kClrModalCloseAct= IM_COL32(200,  50,  50, 255);  // active


// ── Global ImGui style (applied in main.cpp) ──────────────────────────────────
static constexpr float  kStyleWindowRounding    = 0.f;
static constexpr float  kStyleFrameRounding     = 0.f;
static constexpr float  kStyleScrollbarRounding  = 2.f;
static constexpr float  kStyleTabRounding        = 3.f;
static constexpr ImVec4 kStyleWindowBg  = {0.067f, 0.067f, 0.086f, 1.f};  // cool blue-grey — switch button tone
static constexpr ImVec4 kStyleTitleBg   = {0.067f, 0.067f, 0.086f, 1.f};
static constexpr ImVec4 kStyleTitleBgAct= {0.10f,  0.10f,  0.13f,  1.f};

// ── Widgets ───────────────────────────────────────────────────────────────────

// Full-width 1px divider line with a 20px fade-to-transparent on each end.
// col  — line colour (typically ImGuiCol_TabSelected)
// y    — screen Y position of the line
// w    — total width to span
inline void DrawFadeDivider(ImDrawList* dl, ImU32 col, float y, float w)
{
    constexpr float kFade = 20.f;
    ImU32 fade = col & 0x00FFFFFFu;
    float y1   = y + 1.f;
    dl->AddRectFilledMultiColor({0.f,      y}, {kFade,     y1}, fade, col,  col,  fade);
    dl->AddRectFilled          ({kFade,    y}, {w - kFade, y1}, col);
    dl->AddRectFilledMultiColor({w - kFade,y}, {w,         y1}, col,  fade, fade, col);
}
