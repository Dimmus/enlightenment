#define E_IPC_OP_MODULE_LOAD 1
#define E_IPC_OP_MODULE_UNLOAD 2
#define E_IPC_OP_MODULE_ENABLE 3
#define E_IPC_OP_MODULE_DISABLE 4
#define E_IPC_OP_MODULE_LIST 5
#define E_IPC_OP_MODULE_LIST_REPLY 6
#define E_IPC_OP_BG_SET 7
#define E_IPC_OP_BG_GET 8
#define E_IPC_OP_BG_GET_REPLY 9
#define E_IPC_OP_FONT_AVAILABLE_LIST 10
#define E_IPC_OP_FONT_AVAILABLE_LIST_REPLY 11
#define E_IPC_OP_FONT_APPLY 12
#define E_IPC_OP_FONT_FALLBACK_CLEAR 13
#define E_IPC_OP_FONT_FALLBACK_APPEND 14
#define E_IPC_OP_FONT_FALLBACK_PREPEND 15
#define E_IPC_OP_FONT_FALLBACK_LIST 16
#define E_IPC_OP_FONT_FALLBACK_LIST_REPLY 17
#define E_IPC_OP_FONT_FALLBACK_REMOVE 18
#define E_IPC_OP_FONT_DEFAULT_SET 19
#define E_IPC_OP_FONT_DEFAULT_GET 20
#define E_IPC_OP_FONT_DEFAULT_GET_REPLY 21
#define E_IPC_OP_FONT_DEFAULT_REMOVE 22
#define E_IPC_OP_FONT_DEFAULT_LIST 23
#define E_IPC_OP_FONT_DEFAULT_LIST_REPLY 24
#define E_IPC_OP_RESTART 25
#define E_IPC_OP_SHUTDOWN 26
#define E_IPC_OP_LANG_LIST 27
#define E_IPC_OP_LANG_LIST_REPLY 28
#define E_IPC_OP_LANG_SET 29
#define E_IPC_OP_LANG_GET 30
#define E_IPC_OP_LANG_GET_REPLY 31
#define E_IPC_OP_BINDING_MOUSE_LIST 32
#define E_IPC_OP_BINDING_MOUSE_LIST_REPLY 33
#define E_IPC_OP_BINDING_MOUSE_ADD 34
#define E_IPC_OP_BINDING_MOUSE_DEL 35
#define E_IPC_OP_BINDING_KEY_LIST 36
#define E_IPC_OP_BINDING_KEY_LIST_REPLY 37
#define E_IPC_OP_BINDING_KEY_ADD 38
#define E_IPC_OP_BINDING_KEY_DEL 39
#define E_IPC_OP_MENUS_SCROLL_SPEED_SET 40
#define E_IPC_OP_MENUS_SCROLL_SPEED_GET 41
#define E_IPC_OP_MENUS_SCROLL_SPEED_GET_REPLY 42
#define E_IPC_OP_MENUS_FAST_MOVE_THRESHHOLD_SET 43
#define E_IPC_OP_MENUS_FAST_MOVE_THRESHHOLD_GET 44
#define E_IPC_OP_MENUS_FAST_MOVE_THRESHHOLD_GET_REPLY 45
#define E_IPC_OP_MENUS_CLICK_DRAG_TIMEOUT_SET 46
#define E_IPC_OP_MENUS_CLICK_DRAG_TIMEOUT_GET 47
#define E_IPC_OP_MENUS_CLICK_DRAG_TIMEOUT_GET_REPLY 48
#define E_IPC_OP_BORDER_SHADE_ANIMATE_SET 49
#define E_IPC_OP_BORDER_SHADE_ANIMATE_GET 50
#define E_IPC_OP_BORDER_SHADE_ANIMATE_GET_REPLY 51
#define E_IPC_OP_BORDER_SHADE_TRANSITION_SET 52
#define E_IPC_OP_BORDER_SHADE_TRANSITION_GET 53
#define E_IPC_OP_BORDER_SHADE_TRANSITION_GET_REPLY 54
#define E_IPC_OP_BORDER_SHADE_SPEED_SET 55
#define E_IPC_OP_BORDER_SHADE_SPEED_GET 56
#define E_IPC_OP_BORDER_SHADE_SPEED_GET_REPLY 57
#define E_IPC_OP_FRAMERATE_SET 58
#define E_IPC_OP_FRAMERATE_GET 59
#define E_IPC_OP_FRAMERATE_GET_REPLY 60
#define E_IPC_OP_IMAGE_CACHE_SET 61
#define E_IPC_OP_IMAGE_CACHE_GET 62
#define E_IPC_OP_IMAGE_CACHE_GET_REPLY 63
#define E_IPC_OP_FONT_CACHE_SET 64
#define E_IPC_OP_FONT_CACHE_GET 65
#define E_IPC_OP_FONT_CACHE_GET_REPLY 66
#define E_IPC_OP_USE_EDGE_FLIP_SET 67
#define E_IPC_OP_USE_EDGE_FLIP_GET 68
#define E_IPC_OP_USE_EDGE_FLIP_GET_REPLY 69
#define E_IPC_OP_EDGE_FLIP_TIMEOUT_SET 70
#define E_IPC_OP_EDGE_FLIP_TIMEOUT_GET 71
#define E_IPC_OP_EDGE_FLIP_TIMEOUT_GET_REPLY 72
#define E_IPC_OP_MODULE_DIRS_LIST 73
#define E_IPC_OP_MODULE_DIRS_LIST_REPLY 74
#define E_IPC_OP_MODULE_DIRS_APPEND 75
#define E_IPC_OP_MODULE_DIRS_PREPEND 76
#define E_IPC_OP_MODULE_DIRS_REMOVE 77
#define E_IPC_OP_THEME_DIRS_LIST 78
#define E_IPC_OP_THEME_DIRS_LIST_REPLY 79
#define E_IPC_OP_THEME_DIRS_APPEND 80
#define E_IPC_OP_THEME_DIRS_PREPEND 81
#define E_IPC_OP_THEME_DIRS_REMOVE 82
#define E_IPC_OP_FONT_DIRS_LIST 83
#define E_IPC_OP_FONT_DIRS_LIST_REPLY 84
#define E_IPC_OP_FONT_DIRS_APPEND 85
#define E_IPC_OP_FONT_DIRS_PREPEND 86
#define E_IPC_OP_FONT_DIRS_REMOVE 87
#define E_IPC_OP_DATA_DIRS_LIST 88
#define E_IPC_OP_DATA_DIRS_LIST_REPLY 89
#define E_IPC_OP_DATA_DIRS_APPEND 90
#define E_IPC_OP_DATA_DIRS_PREPEND 91
#define E_IPC_OP_DATA_DIRS_REMOVE 92
#define E_IPC_OP_IMAGE_DIRS_LIST 93
#define E_IPC_OP_IMAGE_DIRS_LIST_REPLY 94
#define E_IPC_OP_IMAGE_DIRS_APPEND 95
#define E_IPC_OP_IMAGE_DIRS_PREPEND 96
#define E_IPC_OP_IMAGE_DIRS_REMOVE 97
#define E_IPC_OP_INIT_DIRS_LIST 98
#define E_IPC_OP_INIT_DIRS_LIST_REPLY 99
#define E_IPC_OP_INIT_DIRS_APPEND 100
#define E_IPC_OP_INIT_DIRS_PREPEND 101
#define E_IPC_OP_INIT_DIRS_REMOVE 102
#define E_IPC_OP_ICON_DIRS_LIST 103
#define E_IPC_OP_ICON_DIRS_LIST_REPLY 104
#define E_IPC_OP_ICON_DIRS_APPEND 105
#define E_IPC_OP_ICON_DIRS_PREPEND 106
#define E_IPC_OP_ICON_DIRS_REMOVE 107
#define E_IPC_OP_BG_DIRS_LIST 108
#define E_IPC_OP_BG_DIRS_LIST_REPLY 109
#define E_IPC_OP_BG_DIRS_APPEND 110
#define E_IPC_OP_BG_DIRS_PREPEND 111
#define E_IPC_OP_BG_DIRS_REMOVE 112
#define E_IPC_OP_DESKS_SET 113
#define E_IPC_OP_DESKS_GET 114
#define E_IPC_OP_DESKS_GET_REPLY 115
#define E_IPC_OP_FOCUS_POLICY_SET 116
#define E_IPC_OP_FOCUS_POLICY_GET 117
#define E_IPC_OP_FOCUS_POLICY_GET_REPLY 118
