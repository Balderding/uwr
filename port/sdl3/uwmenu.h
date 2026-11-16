/* uwmenu.h: our Esc menu without the renderer (uwmenu.c), for plat_sdl3.c */
#pragma once
void uwmenu_register(void);                            /* given to uwfront.c (uwf_soft_menu), before the loop */
/* the menu drawn since the last call (lw x lh, 0xAABBGGRR, straight alpha), to lay over the picture; NULL none */
const unsigned char *uwmenu_layer(int *w, int *h);
