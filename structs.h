#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <stdint.h>

// INTERNAL -- IGNORE

typedef enum {
    WINDOW_LAYOUT_TILED_MASTER = 0,
    WINDOW_LAYOUT_HORIZONTAL_MASTER,
    WINDOW_LAYOUT_HORIZONTAL_STRIPES,
    WINDOW_LAYOUT_VERTICAL_STRIPES,
    WINDOW_LAYOUT_FLOATING
} WindowLayout;

typedef struct {
    XftFont* font;
    XftColor color;
    XftDraw* draw;
} FontStruct;

typedef struct {
    uint32_t width, height;
} Monitor;
typedef struct {
    const char* cmd;
    const char* color;
    int32_t bg_color;
    XftColor _xcolor;
    char _text[512];
} BarCommand;

typedef struct {
    const char* cmd;
    const char* icon;
    Window win;
    FontStruct font;
    uint32_t color;
} BarButton;

typedef struct {
    const char* icon;
    const char* color;
    XftColor _xcolor;
} BarDesktopIcon;

typedef enum {
    DESIGN_STRAIGHT = 0,
    DESIGN_SHARP_LEFT_UP,         
    DESIGN_SHARP_RIGHT_UP,
    DESIGN_SHARP_LEFT_DOWN,
    DESIGN_SHARP_RIGHT_DOWN,
    DESIGN_ARROW_LEFT,                        
    DESIGN_ARROW_RIGHT,
    DESIGN_ROUND_LEFT,
    DESIGN_ROUND_RIGHT,
} BarLabelDesign;

typedef struct {
    const char* cmd;
    uint8_t key;
    bool spawned, hidden;
    Window win, frame;
} ScratchpadDef;

typedef struct {
    const char* cmd; 
    uint32_t key;
} Keybind;
