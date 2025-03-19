#include "config.h"
#include "funcs.h"
#include <ctype.h>
#include <libconfig.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xcb/xcb.h>
#include <X11/XF86keysym.h>
#include <xcb/xproto.h>

typedef struct {
    const char *name;
    keycode_t value;
} key_mapping_t;

typedef struct {
    const char *name;
  keycallback_t cb;
} key_cb_mapping_t;

const key_mapping_t keymappings[] = {
    {"KeyVoidSymbol", KeyVoidSymbol},
    {"KeyBackSpace", KeyBackSpace},
    {"KeyTab", KeyTab},
    {"KeyLinefeed", KeyLinefeed},
    {"KeyClear", KeyClear},
    {"KeyReturn", KeyReturn},
    {"KeyPause", KeyPause},
    {"KeyScroll_Lock", KeyScroll_Lock},
    {"KeySys_Req", KeySys_Req},
    {"KeyEscape", KeyEscape},
    {"KeyDelete", KeyDelete},
    {"KeyHome", KeyHome},
    {"KeyLeft", KeyLeft},
    {"KeyUp", KeyUp},
    {"KeyRight", KeyRight},
    {"KeyDown", KeyDown},
    {"KeyPage_Up", KeyPage_Up},
    {"KeyPage_Down", KeyPage_Down},
    {"KeyEnd", KeyEnd},
    {"KeyBegin", KeyBegin},
    {"KeySpace", KeySpace},
    {"KeyExclam", KeyExclam},
    {"KeyQuotedbl", KeyQuotedbl},
    {"KeyNumberSign", KeyNumberSign},
    {"KeyDollar", KeyDollar},
    {"KeyPercent", KeyPercent},
    {"KeyAmpersand", KeyAmpersand},
    {"KeyApostrophe", KeyApostrophe},
    {"KeyParenleft", KeyParenleft},
    {"KeyParenright", KeyParenright},
    {"KeyAsterisk", KeyAsterisk},
    {"KeyPlus", KeyPlus},
    {"KeyComma", KeyComma},
    {"KeyMinus", KeyMinus},
    {"KeyPeriod", KeyPeriod},
    {"KeySlash", KeySlash},
    {"Key0", Key0},
    {"Key1", Key1},
    {"Key2", Key2},
    {"Key3", Key3},
    {"Key4", Key4},
    {"Key5", Key5},
    {"Key6", Key6},
    {"Key7", Key7},
    {"Key8", Key8},
    {"Key9", Key9},
    {"KeyColon", KeyColon},
    {"KeySemicolon", KeySemicolon},
    {"KeyLess", KeyLess},
    {"KeyEqual", KeyEqual},
    {"KeyGreater", KeyGreater},
    {"KeyQuestion", KeyQuestion},
    {"KeyAt", KeyAt},
    {"KeyBracketLeft", KeyBracketLeft},
    {"KeyBackslash", KeyBackslash},
    {"KeyBracketRight", KeyBracketRight},
    {"KeyAsciiCircum", KeyAsciiCircum},
    {"KeyUnderscore", KeyUnderscore},
    {"KeyGrave", KeyGrave},
    {"KeyA", KeyA},
    {"KeyB", KeyB},
    {"KeyC", KeyC},
    {"KeyD", KeyD},
    {"KeyE", KeyE},
    {"KeyF", KeyF},
    {"KeyG", KeyG},
    {"KeyH", KeyH},
    {"KeyI", KeyI},
    {"KeyJ", KeyJ},
    {"KeyK", KeyK},
    {"KeyL", KeyL},
    {"KeyM", KeyM},
    {"KeyN", KeyN},
    {"KeyO", KeyO},
    {"KeyP", KeyP},
    {"KeyQ", KeyQ},
    {"KeyR", KeyR},
    {"KeyS", KeyS},
    {"KeyT", KeyT},
    {"KeyU", KeyU},
    {"KeyV", KeyV},
    {"KeyW", KeyW},
    {"KeyX", KeyX},
    {"KeyY", KeyY},
    {"KeyZ", KeyZ},
    {"KeyBraceLeft", KeyBraceLeft},
    {"KeyBar", KeyBar},
    {"KeyBraceRight", KeyBraceRight},
    {"KeyAsciiTilde", KeyAsciiTilde},
    {"KeyNum_Lock", KeyNum_Lock},
    {"KeyKP_0", KeyKP_0},
    {"KeyKP_1", KeyKP_1},
    {"KeyKP_2", KeyKP_2},
    {"KeyKP_3", KeyKP_3},
    {"KeyKP_4", KeyKP_4},
    {"KeyKP_5", KeyKP_5},
    {"KeyKP_6", KeyKP_6},
    {"KeyKP_7", KeyKP_7},
    {"KeyKP_8", KeyKP_8},
    {"KeyKP_9", KeyKP_9},
    {"KeyKP_Decimal", KeyKP_Decimal},
    {"KeyKP_Divide", KeyKP_Divide},
    {"KeyKP_Multiply", KeyKP_Multiply},
    {"KeyKP_Subtract", KeyKP_Subtract},
    {"KeyKP_Add", KeyKP_Add},
    {"KeyKP_Enter", KeyKP_Enter},
    {"KeyKP_Equal", KeyKP_Equal},
    {"KeyF1", KeyF1},
    {"KeyF2", KeyF2},
    {"KeyF3", KeyF3},
    {"KeyF4", KeyF4},
    {"KeyF5", KeyF5},
    {"KeyF6", KeyF6},
    {"KeyF7", KeyF7},
    {"KeyF8", KeyF8},
    {"KeyF9", KeyF9},
    {"KeyF10", KeyF10},
    {"KeyF11", KeyF11},
    {"KeyF12", KeyF12},
    {"KeyF13", KeyF13},
    {"KeyF14", KeyF14},
    {"KeyF15", KeyF15},
    {"KeyF16", KeyF16},
    {"KeyF17", KeyF17},
    {"KeyF18", KeyF18},
    {"KeyF19", KeyF19},
    {"KeyF20", KeyF20},
    {"KeyF21", KeyF21},
    {"KeyF22", KeyF22},
    {"KeyF23", KeyF23},
    {"KeyF24", KeyF24},
    {"KeyF25", KeyF25},
    {"KeyF26", KeyF26},
    {"KeyF27", KeyF27},
    {"KeyF28", KeyF28},
    {"KeyF29", KeyF29},
    {"KeyF30", KeyF30},
    {"KeyF31", KeyF31},
    {"KeyF32", KeyF32},
    {"KeyF33", KeyF33},
    {"KeyF34", KeyF34},
    {"KeyF35", KeyF35},
    {"KeyShift_L", KeyShift_L},
    {"KeyShift_R", KeyShift_R},
    {"KeyControl_L", KeyControl_L},
    {"KeyControl_R", KeyControl_R},
    {"KeyCaps_Lock", KeyCaps_Lock},
    {"KeyShift_Lock", KeyShift_Lock},
    {"KeyMeta_L", KeyMeta_L},
    {"KeyMeta_R", KeyMeta_R},
    {"KeyAlt_L", KeyAlt_L},
    {"KeyAlt_R", KeyAlt_R},
    {"KeySuper_L", KeySuper_L},
    {"KeySuper_R", KeySuper_R},
    {"KeyHyper_L", KeyHyper_L},
    {"KeyHyper_R", KeyHyper_R},
    {"KeyLowerAudio", KeyHyper_R},

  
    {"KeyAudioLowerVolume", XF86XK_AudioLowerVolume},
    {"KeyAudioRaiseVolume", XF86XK_AudioRaiseVolume},
    {"KeyAudioMute", XF86XK_AudioMute},
    {"KeyAudioPlay", XF86XK_AudioPlay},
    {"KeyAudioStop", XF86XK_AudioStop},
    {"KeyAudioPrev", XF86XK_AudioPrev},
    {"KeyAudioNext", XF86XK_AudioNext},
    {"KeyAudioRecord", XF86XK_AudioRecord},
    {"KeyAudioRewind", XF86XK_AudioRewind},
    {"KeyAudioForward", XF86XK_AudioForward},
    {"KeyAudioPause", XF86XK_AudioPause},

    {"KeyLaunchMail", XF86XK_Mail},
    {"KeyLaunchMedia", XF86XK_AudioMedia},
    {"KeyLaunchHome", XF86XK_HomePage},
    {"KeyLaunchFavorites", XF86XK_Favorites},
    {"KeyLaunchSearch", XF86XK_Search},

    {"KeyBrightnessUp", XF86XK_MonBrightnessUp},
    {"KeyBrightnessDown", XF86XK_MonBrightnessDown},
    {"KeyScreenSaver", XF86XK_ScreenSaver},
    {"KeySleep", XF86XK_Sleep},
    {"KeyPowerOff", XF86XK_PowerOff},
    {"KeyWakeUp", XF86XK_WakeUp},
    
    {"KeyCalculator", XF86XK_Calculator},
    {"KeyFileManager", XF86XK_Explorer},
    {"KeyTerminal", XF86XK_Terminal},
    {"KeyWWW", XF86XK_WWW},
    {"KeyMail", XF86XK_Mail},
    
    {"KeyBattery", XF86XK_Battery},
    {"KeyBluetooth", XF86XK_Bluetooth},
    {"KeyWLAN", XF86XK_WLAN},
    {"KeyTouchpadToggle", XF86XK_TouchpadToggle},
    {"KeyTouchpadOn", XF86XK_TouchpadOn},
    {"KeyTouchpadOff", XF86XK_TouchpadOff},

    {"KeyEject", XF86XK_Eject},
    {"KeyRotateWindows", XF86XK_RotateWindows},
    {"KeyClose", XF86XK_Close},

    {"KeyAudioMicMute", XF86XK_AudioMicMute},
    {"KeyKbdBrightnessUp", XF86XK_KbdBrightnessUp},
    {"KeyKbdBrightnessDown", XF86XK_KbdBrightnessDown},
    {"KeyKbdLightOnOff", XF86XK_KbdLightOnOff}
};
const key_cb_mapping_t keycbmappings[] = {
    {"terminate_successfully", terminate_successfully},
    {"cyclefocusdown", cyclefocusdown},
    {"cyclefocusup", cyclefocusup},
    {"killfocus", killfocus},
    {"togglefullscreen", togglefullscreen},
    {"raisefocus", raisefocus},
    {"cycledesktopup", cycledesktopup},
    {"cycledesktopdown", cycledesktopdown},
    {"cyclefocusdesktopup", cyclefocusdesktopup},
    {"cyclefocusdesktopdown", cyclefocusdesktopdown},
    {"switchdesktop", switchdesktop},
    {"switchfocusdesktop", switchfocusdesktop},
    {"runcmd", runcmd},
    {"addfocustolayout", addfocustolayout},
    {"settiledmaster", settiledmaster},
    {"setverticalstripes", setverticalstripes},
    {"sethorizontalstripes", sethorizontalstripes},
    {"setfloatingmode", setfloatingmode},
    {"updatebarslayout", updatebarslayout},
    {"cycledownlayout", cycledownlayout},
    {"cycleuplayout", cycleuplayout},
    {"addmasterlayout", addmasterlayout},
    {"removemasterlayout", removemasterlayout},
    {"incmasterarealayout", incmasterarealayout},
    {"decmasterarealayout", decmasterarealayout},
    {"incgapsizelayout", incgapsizelayout},
    {"decgapsizelayout", decgapsizelayout},
    {"inclayoutsizefocus", inclayoutsizefocus},
    {"declayoutsizefocus", declayoutsizefocus},
    {"movefocusup", movefocusup},
    {"movefocusdown", movefocusdown},
    {"movefocusleft", movefocusleft},
    {"movefocusright", movefocusright},
    {"cyclefocusmonitordown", cyclefocusmonitordown},
    {"cyclefocusmonitorup", cyclefocusmonitorup},
    {"togglescratchpad", togglescratchpad},
    {"reloadconfigfile", reloadconfigfile},
};

static config_t cfghndl;

static char* replaceplaceholder(const char* str, const char* placeholder, const char* value);

static uint16_t kbmodsfromstr(state_t* s, const char* modifiers);
static keycode_t keycodefromstr(const char* keycode);
static keycallback_t keycbfromstr(const char* cbstr);

static bool cfgreadint(state_t* s, int32_t* val, const char* label);
static bool cfgreadfloat(state_t* s, double* val, const char* label);
static bool cfgreadbool(state_t* s, bool* val, const char* label);
static bool cfgreadstr(state_t* s, const char** val, const char* label);

static bool cfgevalkbmod(state_t* s, kb_modifier_t* mod, const char* label);
static bool cfgevalmousebtn(state_t* s, mousebtn_t* btn, const char* label);
static layout_type_t cfgevallayouttype(state_t* s, const char* label);
static char** cfgevalstrarr(state_t* s, const char* label);
static keybind_t* cfgevalkeybinds(state_t* s, uint32_t* numkeybinds, const char* label);

char*
replaceplaceholder(const char* str, const char* placeholder, const char* value) {
  char* result;
  char* ins;
  char* tmp;
  size_t len_placeholder;
  size_t len_front;
  size_t count;

  if (!str || !placeholder)
    return NULL;
  len_placeholder = strlen(placeholder);
  if (len_placeholder == 0)
    return NULL;
  if (!value)
    value = "";

  // Count the number of replacements needed
  ins = (char*)str;
  for (count = 0; (tmp = strstr(ins, placeholder)); ++count) {
    ins = tmp + len_placeholder;
  }

  tmp = result = malloc(strlen(str) + (strlen(value) - len_placeholder) * count + 1);
  if (!result)
    return NULL;

  // Replace each occurrence of placeholder with the value
  while (count--) {
    ins = strstr(str, placeholder);
    len_front = ins - str;
    tmp = strncpy(tmp, str, len_front) + len_front;
    tmp = strcpy(tmp, value) + strlen(value);
    str += len_front + len_placeholder; 
  }
  strcpy(tmp, str);
  return result;
}

uint16_t 
kbmodsfromstr(state_t* s, const char* modifiers) {
  uint16_t bitmask = 0;
  char buffer[64]; 
  const char *delim = " |";

  const char* modstr;
  if(!cfgreadstr(s, &modstr, "mod_key")) {
    return bitmask;
  }

  modifiers = replaceplaceholder(modifiers, "%mod_key", modstr);

  while (*modifiers) {
    while (isspace(*modifiers)) modifiers++;

    char *end = buffer;
    while (*modifiers && !strchr(delim, *modifiers)) {
      *end++ = *modifiers++;
    }
    *end = '\0'; 

    if (strcmp(buffer, "Shift") == 0) {
      bitmask |= Shift;
    } else if (strcmp(buffer, "Control") == 0) {
      bitmask |= Control;
    } else if (strcmp(buffer, "Alt") == 0) {
      bitmask |= Alt;
    } else if (strcmp(buffer, "Super") == 0) {
      bitmask |= Super;
    }

    while (isspace(*modifiers) || *modifiers == '|') modifiers++;
  }

  return bitmask;
}

keycode_t 
keycodefromstr(const char* keycode) {
  for (size_t i = 0; i < sizeof(keymappings) / sizeof(keymappings[0]); ++i) {
    if (strcmp(keycode, keymappings[i].name) == 0) {
      return keymappings[i].value;
    }
  }
  return -1; 
}

keycallback_t 
keycbfromstr(const char* cbstr) {
  for (size_t i = 0; i < sizeof(keycbmappings) / sizeof(keycbmappings[0]); ++i) {
    if (strcmp(cbstr, keycbmappings[i].name) == 0) {
      return keycbmappings[i].cb;
    }
  }
  return NULL; 
}

bool
cfgreadint(state_t* s, int32_t* val ,const char* label) {
  bool success = (bool)config_lookup_int(&cfghndl, label, val);

  if(!success) {
    logmsg(s, LogLevelError, "config: %s is not set.", label);
  }

  return success;
}

bool 
cfgreadfloat(state_t* s, double* val, const char* label) {
  bool success = (bool)config_lookup_float(&cfghndl, label, val);

  if(!success) {
    logmsg(s, LogLevelError, "config: %s is not set.", label);
  }

  return success;
}

bool 
cfgreadbool(state_t* s, bool* val, const char* label) {
  bool success = (bool)config_lookup_bool(&cfghndl, label, (int32_t*)val);

  if(!success) {
    logmsg(s, LogLevelError, "config: %s is not set.", label);
  }

  return success;
}

bool 
cfgreadstr(state_t* s, const char** val, const char* label) {
  bool success = (bool)config_lookup_string(&cfghndl, label, val);

  if(!success) {
    logmsg(s, LogLevelError, "config: %s is not set.", label);
  }

  return success;
}

bool
cfgevalkbmod(state_t* s, kb_modifier_t* mod, const char* label) {
  bool success = false;

  const char* modstr;
  success = cfgreadstr(s, &modstr, label);

  if(strcmp(modstr, "Shift") == 0) {
    *mod = Shift;
    return success;
  }
  if(strcmp(modstr, "Control") == 0) {
    *mod = Control;
    return success;
  }
  if(strcmp(modstr, "Alt") == 0) {
    *mod = Alt;
    return success;
  }
  if(strcmp(modstr, "Super") == 0) {
    *mod = Super;
    return success;
  }

  logmsg(s, LogLevelError, "config: invalid keyboard modifier specified.");
  return false;
}

bool 
cfgevalmousebtn(state_t* s, mousebtn_t* btn, const char* label) {
  bool success = false;

  const char* btnstr;
  success = cfgreadstr(s, &btnstr, label);

  if(strcmp(btnstr, "LeftMouse") == 0) {
    *btn = LeftMouse; 
  } else if(strcmp(btnstr, "MiddleMouse") == 0) {
    *btn = MiddleMouse;
    return success;
  } else if(strcmp(btnstr, "RightMouse") == 0) {
    *btn = RightMouse;
    return success;
  } else {
    logmsg(s, LogLevelError, "config: invalid mouse button specified.");
    return false;
  }
  return false;
}

layout_type_t
cfgevallayouttype(state_t* s, const char* label) {

  const char* layoutstr;
  cfgreadstr(s, &layoutstr, label);

  layout_type_t layout;

  if(strcmp(layoutstr, "LayoutFloating") == 0) {
    layout = LayoutFloating;
  } else if(strcmp(layoutstr, "LayoutTiledMaster") == 0) {
    layout = LayoutTiledMaster;
  } else if(strcmp(layoutstr, "LayoutHorizontalStripes") == 0) {
    layout = LayoutHorizontalStripes;
  } else if(strcmp(layoutstr, "LayoutVerticalStripes") == 0) {
    layout = LayoutVerticalStripes;
  } else {
    layout = -1;
    logmsg(s, LogLevelError, "config: invalid layout type specified.");
  }


  return layout;
}

char**
cfgevalstrarr(state_t* s, const char* label) {
  const config_setting_t* setting;

  setting = config_lookup(&cfghndl, label);
  if(!setting) {
    logmsg(s, LogLevelError, "config: %s is not set.", label);
    return NULL;
  }

  uint32_t len = (uint32_t)config_setting_length(setting);

  char** arr = malloc(len * sizeof(char*));
  for (uint32_t i = 0; i < len; i++) {
    const char* val = config_setting_get_string_elem(setting, i);
    if (val) {
      arr[i] = malloc(strlen(val) + 1);
      strcpy(arr[i], val);
    }
  } 

  return arr;
}

keybind_t* 
cfgevalkeybinds(state_t* s, uint32_t* numkeybinds, const char* label) {
  const config_setting_t* setting;
  setting = config_lookup(&cfghndl, "keybinds");
  if(!setting) {
    logmsg(s, LogLevelError, "config: %s is not set.", label);
    return NULL;
  }
  uint32_t len = config_setting_length(setting);
  *numkeybinds = len;
  keybind_t* keys = malloc(sizeof(keybind_t) * len);

  for(uint32_t i = 0; i < len; ++i)
  {
    config_setting_t* keybind = config_setting_get_elem(setting, i);

    const char* mods_str;
    const char* key_str;
    const char* do_str;

    config_setting_lookup_string(keybind, "mod", &mods_str);
    config_setting_lookup_string(keybind, "key", &key_str);
    config_setting_lookup_string(keybind, "do", &do_str);

    uint16_t mods = kbmodsfromstr(s, mods_str);
    keycode_t key = keycodefromstr(key_str);
    keycallback_t cb = keycbfromstr(do_str);

    if((int32_t)key == -1) {
      logmsg(s, LogLevelError, "config: Invalid keycode '%s'.", key_str);
      continue;
    }

    if(!cb) {
      logmsg(s, LogLevelError, "config: Invalid key action '%s'.", do_str);
      continue;
    }
    const char* cmd = NULL;
    config_setting_lookup_string(keybind, "cmd", &cmd);

    uint32_t i_val = 0;
    config_setting_lookup_int(keybind, "i", (int32_t*)&i_val);

    keys[i] = (keybind_t){
      .cb = cb,
      .key = key,
      .data = (passthrough_data_t){
        .i = i_val,
        .cmd = cmd
      },
      .modmask = mods
    };
  }

  return keys;
}


void 
initconfig(state_t* s) {
  config_init(&cfghndl);

  const char* home = getenv("HOME");
  if(!home) {
    logmsg(s, LogLevelError, "cannot read config file because HOME is not defined.");
  }

  char *cfg_path;
  char const cfg_path_global[] = "/etc/ragnarwm/ragnar.cfg";

  asprintf(&cfg_path, "%s/.config/ragnarwm/ragnar.cfg", home);

  if (
    !config_read_file(&cfghndl, cfg_path)
    && !config_read_file(&cfghndl, cfg_path_global)
  ) {
    logmsg(s, LogLevelError, "%s:%d - %s\n", config_error_file(&cfghndl),
            config_error_line(&cfghndl), config_error_text(&cfghndl));

    destroyconfig();
    terminate(s, EXIT_FAILURE);
  }

}

void 
readconfig(state_t* s, config_data_t* data) {
  if(!data) return;

  bool success = false;

  success = cfgreadint(s, (int32_t*)&data->maxstruts, "max_struts");
  success = cfgreadint(s, (int32_t*)&data->maxdesktops, "num_desktops");
  success = cfgreadint(s, (int32_t*)&data->maxscratchpads, "max_scratchpads");

  success = cfgreadint(s, (int32_t*)&data->winborderwidth, "win_border_width");
  success = cfgreadint(s, (int32_t*)&data->winbordercolor, "win_border_color");
  success = cfgreadint(s, (int32_t*)&data->winbordercolor_selected, "win_border_color_selected");

  success = cfgevalkbmod(s, &data->modkey, "mod_key"); 
  success = cfgevalkbmod(s, &data->winmod, "win_mod"); 

  success = cfgevalmousebtn(s, &data->movebtn, "move_button");
  success = cfgevalmousebtn(s, &data->resizebtn, "resize_button");

  success = cfgreadint(s, (int32_t*)&data->desktopinit, "initial_desktop");

  data->desktopnames = cfgevalstrarr(s, "desktop_names");
  success = data->desktopnames != NULL;

  success = cfgreadbool(s, &data->usedecoration, "use_decoration");

  success = cfgreadfloat(s, &data->layoutmasterarea, "layout_master_area");
  success = cfgreadfloat(s, &data->layoutmasterarea_min, "layout_master_area_min");
  success = cfgreadfloat(s, &data->layoutmasterarea_max, "layout_master_area_max");
  success = cfgreadfloat(s, &data->layoutmasterarea_step, "layout_master_area_step");

  success = cfgreadfloat(s, &data->layoutsize_step, "layout_size_step");
  success = cfgreadfloat(s, &data->layoutsize_min, "layout_size_min");

  success = cfgreadfloat(s, &data->keywinmove_step, "key_win_move_step");


  success = cfgreadint(s, (int32_t*)&data->winlayoutgap, "win_layout_gap");
  success = cfgreadint(s, (int32_t*)&data->winlayoutgap_max, "win_layout_gap_max");
  success = cfgreadint(s, (int32_t*)&data->winlayoutgap_step, "win_layout_gap_step");

  data->initlayout = cfgevallayouttype(s, "initial_layout");

  success = cfgreadint(s, (int32_t*)&data->motion_notify_debounce_fps, "motion_notify_debounce_fps");
  success = cfgreadbool(s, &data->glvsync, "gl_vsync");

  success = cfgreadstr(s, (const char**)&data->cursorimage, "cursor_image");

  const char* home = getenv("HOME");
  const char* relpath = "/.ragnarwm.log";
  char* logpath = malloc(strlen(home) + strlen(relpath) + 2);
  sprintf(logpath, "%s%s", home, relpath);
  data->logfile = logpath;

  success = cfgreadbool(s, &data->logmessages, "log_messages");
  success = cfgreadbool(s, &data->shouldlogtofile, "should_log_to_file");


  data->keybinds = cfgevalkeybinds(s, (uint32_t*)&data->numkeybinds, "keybinds");

  success = data->keybinds != NULL;

  if(!success) {
    terminate(s, EXIT_FAILURE);
  }
}

void 
reloadconfig(state_t* s, config_data_t* data) {
  destroyconfig();

  bool using_decoration = s->config.usedecoration;

  initconfig(s);
  readconfig(s, data);

  // Load the default root cursor image
  loaddefaultcursor(s);

  // Grab the window manager's keybinds
  grabkeybinds(s);

  if(!using_decoration) {
    s->config.usedecoration = false;
  }

  // Reload struts 
  s->nwinstruts = 0;
  getwinstruts(s, s->root);

  // Reload desktops
  for(monitor_t* mon = s->monitors; mon != NULL; mon = mon->next) {
    for(uint32_t i = 0; i < s->config.maxdesktops; i++) {
      mon->layouts[i].nmaster = 1;
      mon->layouts[i].gapsize = s->config.winlayoutgap;
      mon->layouts[i].masterarea = MIN(MAX(s->config.layoutmasterarea, 0.0), 1.0);
      mon->layouts[i].curlayout = s->config.initlayout;
    }

    uint32_t lastdesktopcount = mon->desktopcount;
    uint32_t curdesktop = mondesktop(s, mon)->idx;

    mon->desktopcount = 0;
    free(mon->activedesktops);
    mon->activedesktops = malloc(sizeof(*mon->activedesktops) * s->config.maxdesktops);
    for(uint32_t i = 0; i < s->config.maxdesktops; i++) {   
      createdesktop(s, i, mon);
      mon->activedesktops[i].init = true; 
    }

    uint32_t desktopcount = 0;
    for(uint32_t i = 0; i < mon->desktopcount; i++) {
      if(mon->activedesktops[i].init) {
        desktopcount++;
      }
    }
    xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHnumberOfDesktops],
                        XCB_ATOM_CARDINAL, 32, 1, &desktopcount);
    uploaddesktopnames(s, mon);

    if(lastdesktopcount > mon->desktopcount) {
      if(curdesktop > mon->desktopcount) {
        switchdesktop(s, (passthrough_data_t){.i = mon->desktopcount - 1});
      }
      for(monitor_t* mon = s->monitors; mon != NULL; mon = mon->next) {
        for(client_t* cl = mon->clients; cl != NULL; cl = cl->next) {
          if(cl->mon == mon && cl->desktop >= s->config.maxdesktops) {
            switchclientdesktop(s, cl, s->config.maxdesktops - 1);
          }
        }
      }
    }
  }

  s->scratchpads = realloc(s->scratchpads, sizeof(*s->scratchpads) * s->config.maxscratchpads);

  for(uint32_t i = 0; i < s->config.maxscratchpads; i++) {
    if(s->scratchpads[i].win != 0) {
      xcb_unmap_window(s->con, s->scratchpads[i].win);
      s->scratchpads[i].win = 0;
      s->scratchpads[i].hidden = true;
      s->scratchpads[i].needs_restart = true;
    }
  }

  s->mapping_scratchpad_index = -1;

  for(monitor_t* mon = s->monitors; mon != NULL; mon = mon->next) {
    for(client_t* cl = mon->clients; cl != NULL; cl = cl->next) {
      setbordercolor(s, cl, s->config.winbordercolor);
      setborderwidth(s, cl, s->config.winborderwidth);
    }
  }

  for(monitor_t* mon = s->monitors; mon != NULL; mon = mon->next) {
    makelayout(s, mon);
  }

  xcb_flush(s->con);
}

void 
destroyconfig(void) {
  config_destroy(&cfghndl);
}
