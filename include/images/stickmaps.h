#ifndef GTS_STICKMAPS_H
#define GTS_STICKMAPS_H

// constants for the "stickmaps" in 2d Plot submenu

// Code in use from the PhobGCC project is licensed under GPLv3. A copy of this license is provided in the root
// directory of this project's repository.

// Upstream URL for the PhobGCC project is: https://github.com/PhobGCC/PhobGCC-SW

#include <gctypes.h>

// PhobGCC/rp2040/include/images/await255.h
// 'await255', 255x255px
extern const unsigned char await_indexes[];
extern const unsigned char await_image[];

// PhobGCC/rp2040/include/images/crouch255.h
// 'crouch255', 255x255px
extern const unsigned char crouch_indexes[];
extern const unsigned char crouch_image[];

// PhobGCC/rp2040/include/images/deadzone255.h
// 'deadzone255', 255x255px
extern const unsigned char deadzone_indexes[];
extern const unsigned char deadzone_image[];

// PhobGCC/rp2040/include/images/ledgeL255.h
// 'ledgeL255', 255x255px
extern const unsigned char ledgeL_indexes[];
extern const unsigned char ledgeL_image[];

// PhobGCC/rp2040/include/images/ledgeR255.h
// 'ledgeR255', 255x255px
extern const unsigned char ledgeR_indexes[];
extern const unsigned char ledgeR_image[];

// PhobGCC/rp2040/include/images/movewait255.h
// 'movewait255', 255x255px
extern const unsigned char movewait_indexes[];
extern const unsigned char movewait_image[];

extern const u32 CUSTOM_COLORS[];

#define IMAGE_LEN 7
enum IMAGE { NO_IMAGE, DEADZONE, A_WAIT, MOVE_WAIT, CROUCH, LEDGE_L, LEDGE_R  };

#endif //GTS_STICKMAPS_H
