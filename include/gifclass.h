/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Module:       gifclass.h                                                */
/*    Author:       James                                                     */
/*    Created:      Thu Mar 21 2019                                           */
/*    Description:  C++ wrapper for gif decode                                */
/*                                                                            */
/*----------------------------------------------------------------------------*/
#include "vex.h"

typedef struct gd_Palette {
    int         size;
    uint8_t     colors[0x100 * 3];
} gd_Palette;

/*----------------------------------------------------------------------------*/
// originally in giddec.h
//
typedef struct gd_GCE {
    uint16_t    delay;
    uint8_t     tindex;
    uint8_t     disposal;
    int         input;
    int         transparency;
} gd_GCE;

typedef struct gd_GIF {
    FILE       *fp;
    off_t       anim_start;
    uint16_t    width;
    uint16_t    height;
    uint16_t    depth;
    uint16_t    loop_count;
    gd_GCE      gce;
    gd_Palette *palette;
    gd_Palette  lct;
    gd_Palette  gct;
    
    void (*plain_text)(
      struct gd_GIF *gif,
      uint16_t  tx,
      uint16_t  ty,
      uint16_t  tw,
      uint16_t  th, 
      uint8_t   cw,
      uint8_t   ch,
      uint8_t   fg,
      uint8_t   bg
    );
    
    void (*comment)(struct gd_GIF *gif);
    
    void (*application)(struct gd_GIF *gif, char id[8], char auth[3]);
    
    uint16_t    fx, fy, fw, fh;
    uint8_t     bgindex;
    uint8_t    *canvas;
    uint8_t    *frame;
} gd_GIF;

/*----------------------------------------------------------------------------*/

namespace vex {
  class Gif {
    private:
      gd_GIF          *_gif = NULL;
      int              _sx;
      int              _sy;
      void            *_gifmem = NULL;
      void            *_buffer = NULL;
      int              _frame  = 0;

      vex::timer       _timer;
      vex::brain::lcd  _lcd;
      vex::thread      _t1;

      static int render_task(void *arg );
      void  cleanup();
    
    public:
      Gif( const char *fname, int sx, int sy, bool bMemoryBuffer = true );    
      ~Gif();
      int getFrameIndex();
  };
}