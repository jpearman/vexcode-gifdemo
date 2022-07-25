/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Module:       gifclass.cpp                                              */
/*    Author:       James                                                     */
/*    Created:      Thu Mar 21 2019                                           */
/*    Description:  C++ wrapper for gif decode                                */
/*                                                                            */
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "gifclass.h"

/*----------------------------------------------------------------------------*/
// The following code was originally in a file gifdec.c
// by Marcel Rodrigues from this github repo.
// https://github.com/lecram/gifdec
//

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define BYTES_PER_PIXEL   4

typedef struct Entry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
} Entry;

typedef struct Table {
    int bulk;
    int nentries;
    Entry *entries;
} Table;

static uint16_t
read_num(FILE *fp)
{
    uint8_t bytes[2];

    fread(bytes, 1, 2, fp);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

static gd_GIF *
gd_open_gif( FILE *fp )
{
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    int gct_sz;
    gd_GIF *gif = NULL;
  
    if (fp == NULL) return NULL;
    /* Header */
    fread(sigver, 1, 3, fp);
    if (memcmp(sigver, "GIF", 3) != 0) {
        fprintf(stderr, "invalid signature\n");
        goto fail;
    }
    /* Version */
    fread(sigver, 1, 3, fp);
    if (memcmp(sigver, "89a", 3) != 0) {
        fprintf(stderr, "invalid version\n");
        goto fail;
    }
    /* Width x Height */
    width  = read_num(fp);
    height = read_num(fp);
    /* FDSZ */
    fread(&fdsz, 1, 1, fp);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        fprintf(stderr, "no global color table\n");
        goto fail;
    }
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    fread(&bgidx, 1, 1, fp);
    /* Aspect Ratio */
    fread(&aspect, 1, 1, fp);
    /* Create gd_GIF Structure. */
    gif = (gd_GIF *)calloc(1, sizeof(*gif) + (BYTES_PER_PIXEL+1) * width * height);
    if (!gif) goto fail;
    gif->fp = fp;
    gif->width  = width;
    gif->height = height;
    gif->depth  = depth;
    /* Read GCT */
    gif->gct.size = gct_sz;
    fread(gif->gct.colors, 1, 3 * gif->gct.size, fp);
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;
    gif->canvas = (uint8_t *) &gif[1];
    gif->frame = &gif->canvas[BYTES_PER_PIXEL * width * height];
    if (gif->bgindex)
        memset(gif->frame, gif->bgindex, gif->width * gif->height);
    gif->anim_start = ftell(fp);
    goto ok;
fail:
    fclose(fp);
ok:
    return gif;
}

static void
discard_sub_blocks(gd_GIF *gif)
{
    uint8_t size;

    do {
        fread(&size, 1, 1, gif->fp);
        fseek(gif->fp, size, SEEK_CUR);
    } while (size);
}

static void
read_plain_text_ext(gd_GIF *gif)
{
    if (gif->plain_text) {
        uint16_t tx, ty, tw, th;
        uint8_t cw, ch, fg, bg;
        off_t sub_block;
        fseek(gif->fp, 1, SEEK_CUR); /* block size = 12 */
        tx = read_num(gif->fp);
        ty = read_num(gif->fp);
        tw = read_num(gif->fp);
        th = read_num(gif->fp);
        fread( &cw, 1, 1, gif->fp);
        fread( &ch, 1, 1, gif->fp);
        fread( &fg, 1, 1, gif->fp);
        fread( &bg, 1, 1, gif->fp);
        sub_block = ftell(gif->fp);
        gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
        fseek(gif->fp, sub_block, SEEK_SET);
    } else {
        /* Discard plain text metadata. */
        fseek(gif->fp, 13, SEEK_CUR);
    }
    /* Discard plain text sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_graphic_control_ext(gd_GIF *gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    fseek(gif->fp, 1, SEEK_CUR);
    fread( &rdit, 1, 1, gif->fp);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif->fp);
    fread( &gif->gce.tindex, 1, 1, gif->fp);
    /* Skip block terminator. */
    fseek(gif->fp, 1, SEEK_CUR);
}

static void
read_comment_ext(gd_GIF *gif)
{
    if (gif->comment) {
        off_t sub_block = ftell(gif->fp);
        gif->comment(gif);
        fseek(gif->fp, sub_block, SEEK_SET);
    }
    /* Discard comment sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_application_ext(gd_GIF *gif)
{
    char app_id[8];
    char app_auth_code[3];

    /* Discard block size (always 0x0B). */
    fseek(gif->fp, 1, SEEK_CUR);
    /* Application Identifier. */
    fread( app_id, 1, 8, gif->fp);
    /* Application Authentication Code. */
    fread( app_auth_code, 1, 3, gif->fp);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        fseek(gif->fp, 2, SEEK_CUR);
        gif->loop_count = read_num(gif->fp);
        /* Skip block terminator. */
        fseek(gif->fp, 1, SEEK_CUR);
    } else if (gif->application) {
        off_t sub_block = ftell(gif->fp);
        gif->application(gif, app_id, app_auth_code);
        fseek(gif->fp, sub_block, SEEK_SET);
        discard_sub_blocks(gif);
    } else {
        discard_sub_blocks(gif);
    }
}

static void
read_ext(gd_GIF *gif)
{
    uint8_t label;

    fread( &label, 1, 1, gif->fp);
    switch (label) {
    case 0x01:
        read_plain_text_ext(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE:
        read_comment_ext(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        fprintf(stderr, "unknown extension: %02X\n", label);
    }
}

static Table *
new_table(int key_size)
{
    int key;
    int init_bulk = MAX(1 << (key_size + 1), 0x100);
    Table *table = (Table *)malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
    if (table) {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = (Entry *) &table[1];
        for (key = 0; key < (1 << key_size); key++)
            table->entries[key] = (Entry) {1, 0xFFF, (uint8_t)key};
    }
    return table;
}

/* Add table entry. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table */
static int
add_entry(Table **tablep, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    Table *table = *tablep;
    if (table->nentries == table->bulk) {
        table->bulk *= 2;
        table = (Table *)realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
        if (!table) return -1;
        table->entries = (Entry *) &table[1];
        *tablep = table;
    }
    table->entries[table->nentries] = (Entry) {length, prefix, suffix};
    table->nentries++;
    if ((table->nentries & (table->nentries - 1)) == 0)
        return 1;
    return 0;
}

static uint16_t
get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            /* Update byte. */
            if (*sub_len == 0)
                fread( sub_len, 1, 1, gif->fp); /* Must be nonzero! */
            fread( byte, 1, 1, gif->fp);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

/* Compute output index of y-th input line, in frame of height h. */
static int
interlaced_line_index(int h, int y)
{
    int p; /* number of lines in current pass */

    p = (h - 1) / 8 + 1;
    if (y < p) /* pass 1 */
        return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p) /* pass 2 */
        return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p) /* pass 3 */
        return y * 4 + 2;
    y -= p;
    /* pass 4 */
    return y * 2 + 1;
}

/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image_data(gd_GIF *gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_is_full;
    int frm_off, str_len, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table *table;
    Entry entry;
    off_t start, end;

    fread( &byte, 1, 1, gif->fp);
    key_size = (int) byte;
    start = ftell(gif->fp);
    discard_sub_blocks(gif);
    end = ftell(gif->fp);
    fseek(gif->fp, start, SEEK_SET);
    clear = 1 << key_size;
    stop = clear + 1;
    table = new_table(key_size);
    if(table == NULL) goto fail;
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
    frm_off = 0;
    ret = 0;
    table_is_full = 0;
    str_len = 0;
    while (1) {
        if (key == clear) {
            key_size = init_key_size;
            table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        } else if (!table_is_full) {
            ret = add_entry(&table, str_len + 1, key, entry.suffix);
            if (ret == -1) {
                free(table);
                return -1;
            }
            if (table->nentries == 0x1000) {
                ret = 0;
                table_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear) continue;
        if (key == stop) break;
        if (ret == 1) key_size++;
        entry = table->entries[key];
        str_len = entry.length;
        while (1) {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if (interlace)
                y = interlaced_line_index((int) gif->fh, y);
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
            if (entry.prefix == 0xFFF)
                break;
            else
                entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < table->nentries - 1 && !table_is_full)
            table->entries[table->nentries - 1].suffix = entry.suffix;
    }
    free(table);
    fread( &sub_len, 1, 1, gif->fp); /* Must be zero! */
    fseek(gif->fp, end, SEEK_SET);
    return 0;
fail:
    return -1;
}

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image(gd_GIF *gif)
{
    uint8_t fisrz;
    int interlace;

    /* Image Descriptor. */
    gif->fx = read_num(gif->fp);
    gif->fy = read_num(gif->fp);
    gif->fw = read_num(gif->fp);
    gif->fh = read_num(gif->fp);
    fread( &fisrz, 1, 1, gif->fp);
    interlace = fisrz & 0x40;
    /* Ignore Sort Flag. */
    /* Local Color Table? */
    if (fisrz & 0x80) {
        /* Read LCT */
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        fread( gif->lct.colors, 1, 3 * gif->lct.size, gif->fp);
        gif->palette = &gif->lct;
    } else
        gif->palette = &gif->gct;
    /* Image Data. */
    return read_image_data(gif, interlace);
}

static void
render_frame_rect(gd_GIF *gif, uint8_t *buffer)
{
    int i, j, k;
    uint8_t index, *color;
    i = gif->fy * gif->width + gif->fx;
    for (j = 0; j < gif->fh; j++) {
        for (k = 0; k < gif->fw; k++) {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
            color = &gif->palette->colors[index*3];
            if (!gif->gce.transparency || index != gif->gce.tindex) {
              int offset = (i+k)*BYTES_PER_PIXEL;
              buffer[offset+2] = *(color+0);
              buffer[offset+1] = *(color+1);
              buffer[offset+0] = *(color+2);
            }
        }
        i += gif->width;
    }
}

static void
dispose(gd_GIF *gif)
{
    int i, j, k;
    uint8_t *bgcolor;
    switch (gif->gce.disposal) {
    case 2: /* Restore to background color. */
        bgcolor = &gif->palette->colors[gif->bgindex*3];
        i = gif->fy * gif->width + gif->fx;
        for (j = 0; j < gif->fh; j++) {
            for (k = 0; k < gif->fw; k++) {
              int offset = (i+k)*BYTES_PER_PIXEL;
              gif->canvas[offset+2] = *(bgcolor+0);
              gif->canvas[offset+1] = *(bgcolor+1);
              gif->canvas[offset+0] = *(bgcolor+2);
            }
            i += gif->width;
        }
        break;
    case 3: /* Restore to previous, i.e., don't update canvas.*/
        break;
    default:
        /* Add frame non-transparent pixels to canvas. */
        render_frame_rect(gif, gif->canvas);
    }
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
static int
gd_get_frame(gd_GIF *gif)
{
    char sep;

    dispose(gif);
    fread( &sep, 1, 1, gif->fp);
    while (sep != ',') {
        if (sep == ';')
            return 0;
        if (sep == '!')
            read_ext(gif);
        else return -1;
        fread( &sep, 1, 1, gif->fp);
    }
    if (read_image(gif) == -1)
        return -1;
    return 1;
}

static void
gd_render_frame(gd_GIF *gif, uint8_t *buffer)
{
    memcpy(buffer, gif->canvas, gif->width * gif->height * BYTES_PER_PIXEL);
    render_frame_rect(gif, buffer);
}

static void
gd_rewind(gd_GIF *gif)
{
    fseek(gif->fp, gif->anim_start, SEEK_SET);
}

static void
gd_close_gif(gd_GIF *gif)
{
    fclose(gif->fp);
    free(gif);
}

/*----------------------------------------------------------------------------*/
// C++ wrapper class by James
// 2019 - 2022
// MIT license
//

// static member function to handle rendering frames
//
int
Gif::render_task(void *arg ) {
  if( arg == NULL)
    return(0);
    
  Gif *instance = static_cast<Gif *>(arg);

  gd_GIF *gif = instance->_gif;

  for (unsigned looped = 1;; looped++) {
      int32_t now = instance->_timer.system();
      int err = 0;
      instance->_frame = 0;

      while ((err = gd_get_frame(gif)) > 0) {
          gd_render_frame(gif, (uint8_t *)instance->_buffer);
                    
          instance->_lcd.drawImageFromBuffer((uint32_t *)instance->_buffer, instance->_sx, instance->_sy, gif->width, gif->height );
          instance->_frame++;

          // how long to get, render and draw to screen
          int32_t delay = gif->gce.delay * 10;
        
          // do we need delay to honor loop speed
          int32_t delta = instance->_timer.system() - now;
          delay -= delta;
          if( delay > 0 ) {
            this_thread::sleep_for(delay);
          }

          // for next loop
          now = instance->_timer.system();
      }
      if( err == -1 ) {
        printf("Gif: error\n");
        break;
      }
      // done ?
      if (looped == gif->loop_count) {
        break;
      }

      gd_rewind(gif);
  }
 
  instance->cleanup();

  return(0);
}

Gif::Gif( const char *fname, int sx, int sy, bool bMemoryBuffer ) {
  _sx = sx;
  _sy = sy;
  FILE *fp = fopen( fname, "rb" );

  if( fp != NULL ) {
    // get file length
    fseek( fp, 0, SEEK_END );
    size_t len = ftell( fp );
    fseek( fp, 0, SEEK_SET );

    // optimize by reading into memory buffer ?
    if( bMemoryBuffer ) {
      _gifmem = malloc( len );
      if( _gifmem != NULL ) {
        int nRead = fread( _gifmem, 1, len, fp );
        (void) nRead;
      }
      fclose(fp);
      fp = NULL;
    }

    // using memory, then reopen file
    if( _gifmem != NULL ) {
      // create a FILE from memory buffer
      fp = fmemopen( _gifmem, len, "rb" );
    }

    // good file ?
    if( fp != NULL ) {
      // open gif file
      // will allocate memory for background and one animation
      // frame.
      _gif = gd_open_gif( fp );
      if( _gif == NULL ) {
        return;
      }
      
      // memory for rendering frame
      _buffer = (uint32_t *)malloc(_gif->width * _gif->height * sizeof(uint32_t));
      if( _buffer == NULL ) {
        // out of memory
        gd_close_gif( _gif );
        if( _gifmem ) {
          free(_gifmem);
        }
      }
      else {
        // create thread to handle this gif
        _t1 = thread( render_task, static_cast<void *>(this) );
      }
    }
  }
};

Gif::~Gif() {
  cleanup();
};

// cleanup memory when gif finishs
//

void
Gif::cleanup() {
  // stop thread
  _t1.interrupt();

  if( _buffer ) {
    free(_buffer);
    _buffer = NULL;
  }

  if( _gif ) {
    gd_close_gif(_gif);
    _gif = NULL;
  }

  if( _gifmem ) {
    free(_gifmem);
    _gifmem = NULL;
  }
}

// get current rendered frame
//

int
Gif::getFrameIndex() {
  return _frame;
}