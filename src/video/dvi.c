#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "hardware/structs/bus_ctrl.h"

#include "g_config.h"
#include "dvi.h"
#include "crt.h"
#include "video.pio.h"
#include "v_buf.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

extern settings_t settings;

static int dma_ch0; // data line → conv PIO TX
static int dma_ch1; // control: reloads ch0 read addr, fires IRQ
static int dma_ch2; // out_data: reads palette data → sends to output PIO TX
static int dma_ch3; // set_addr: reads address from conv RX → sets ch2 read addr
static uint offset;
static uint offset_conv;

extern video_mode_t video_mode;
extern int16_t h_visible_area;

static uint32_t *v_out_dma_buf[2];
static uint32_t *v_out_dma_buf_alloc; // single contiguous allocation for both ping-pong buffers
static uint32_t *v_out_sync_hblank;   // pre-filled H-blank line (NO_SYNC + H_SYNC + NO_SYNC)
static uint32_t *v_out_sync_vsync;    // pre-filled V-sync line (V_SYNC + VH_SYNC + V_SYNC)
static uint32_t *v_out_scanline;      // scanline: dimmed copy of the previous line + H-blank (keeps DE high, unlike v_out_sync_hblank)

static uint8_t scanlines_mode = SCANLINES_OFF;

// ISR state (file-scope for reset in stop_dvi)
static uint16_t y = 0;
static uint8_t *scr_buffer = NULL;
static uint32_t active_buf_idx = 0;

static uint32_t pixels[256];
// 4KB-aligned palette: 172 entries × 16 bytes (4 × uint32_t each), 8-bit index
// Each entry: {first pixel lo, hi, second pixel lo, hi}
// Entries 0-15: colors, 16-19: sync patterns, 20-35: dimmed colors for scanlines (set_dim_palette),
// 36-171: BEAM gap colors, one per unordered pair of colors (set_beam_palette)
static uint32_t palette[172 * 4] __attribute__((aligned(4096)));
// a line word holds two palette indices: bits 0-7 (first pixel) and 8-15 (second)
#define IDX2(a, b) ((uint32_t)(a) | ((uint32_t)(b) << 8))
// sync pulse pattern indexes (after 16 color entries)
static const uint8_t NO_SYNC = 16;
static const uint8_t H_SYNC = 17;
static const uint8_t V_SYNC = 18;
static const uint8_t VH_SYNC = 19;
// first dimmed color entry; adding SCANLINE_IDX_ADD to a line word shifts both palette indices to it
static const uint8_t DIM_BASE = 20;
#define SCANLINE_IDX_ADD IDX2(20, 20)
// first BEAM pair entry; beam_lut[(a << 4) | b] gives the entry for colors a and b (symmetric)
static const uint8_t BEAM_BASE = 36;
static uint8_t beam_lut[256];

// CRT effect parameters (normal level, beam width, mask), set by set_all_palettes
static crt_t crt;

static uint64_t get_ser_diff_data(uint16_t dataR, uint16_t dataG, uint16_t dataB)
{
  uint64_t out64 = 0;

  for (int bit = 9; bit >= 0; bit--)
  {
    out64 <<= 6;

    if (bit == 4)
      out64 <<= 2;

    uint8_t bR = (dataR >> bit) & 1;
    uint8_t bG = (dataG >> bit) & 1;
    uint8_t bB = (dataB >> bit) & 1;

#ifndef DVI_PINS_REVERSED
    bR = 2 - bR;
    bG = 2 - bG;
    bB = 2 - bB;

    out64 |= (bB << 4) | (bG << 2) | (bR << 0);
#else
    bR = bR + 1;
    bG = bG + 1;
    bB = bB + 1;

    out64 |= (bR << 4) | (bG << 2) | (bB << 0);
#endif
  }

  return out64;
}

// TMDS encoder
static uint tmds_encoder(uint8_t d8)
{
  int s1 = 0;

  for (int i = 0; i < 8; i++)
    s1 += (d8 & (1u << i)) ? 1 : 0;

  uint8_t xnor = ((s1 > 4) || ((s1 == 4) && ((d8 & 1) == 0))) ? 1 : 0;

  uint16_t d_out = d8 & 1;
  uint16_t qi = d_out;

  for (int i = 1; i < 8; i++)
  {
    d_out |= ((qi << 1) ^ (d8 & (1u << i))) ^ (xnor << i);
    qi = d_out & (1u << i);
  }

  d_out |= xnor ? (1u << 9) : (1u << 8);
  return d_out;
}

// Load a 32-bit value into PIO X register (SM must be stopped or idle)
static void pio_set_x(PIO pio, int sm, uint32_t v)
{
  uint instr_shift = pio_encode_in(pio_x, 4);
  uint instr_mov = pio_encode_mov(pio_x, pio_isr);

  for (int i = 0; i < 8; i++)
  {
    pio_sm_exec(pio, sm, pio_encode_set(pio_x, (v >> (i * 4)) & 0xf));
    pio_sm_exec(pio, sm, instr_shift);
  }

  pio_sm_exec(pio, sm, instr_mov);
}

// TMDS control tokens; a data period must not contain them
static const uint16_t tmds_ctrl[4] = {0x354, 0x0ab, 0x154, 0x2ab};

// TMDS data codes that decode to v (DVI 1.0 decoder): bit 8 selects XOR / XNOR, bit 9 marks bits 0-7 inverted;
// one code per (bit 8, bit 9) combination, keeping only codes with at most 5 transitions between adjacent bits.
// Receivers tell data from control characters (7+ transitions) by that count and use it for character alignment;
// codes with more transitions decode correctly but made the monitor drop the signal. Returns the count (0-4).
static int tmds_codes(uint8_t v, uint16_t *out)
{
  int n = 0;

  for (int m = 0; m < 4; m++)
  {
    bool xnor = m & 1;
    bool inv = m & 2;
    uint16_t q = v & 1;

    for (int i = 1; i < 8; i++)
      q |= ((((q >> (i - 1)) ^ (v >> i)) & 1) ^ xnor) << i;

    uint16_t code = (inv ? (~q & 0xff) : q) | (xnor ? 0 : 1u << 8) | (inv ? 1u << 9 : 0);
    bool ctrl = false;

    for (int i = 0; i < 4; i++)
      ctrl |= code == tmds_ctrl[i];

    if (!ctrl && __builtin_popcount((code ^ (code >> 1)) & 0x1ff) <= 5)
      out[n++] = code;
  }

  return n;
}

static int tmds_disparity(uint16_t code)
{
  return 2 * __builtin_popcount(code) - 10;
}

// zero-disparity codes for levels aa and bb (ca, na and cb, nb from tmds_codes); false if there is none
static bool tmds_pair_codes(const uint16_t *ca, int na, const uint16_t *cb, int nb, uint16_t *code_a, uint16_t *code_b)
{
  for (int i = 0; i < na; i++)
    for (int j = 0; j < nb; j++)
      if (tmds_disparity(ca[i]) + tmds_disparity(cb[j]) == 0)
      {
        *code_a = ca[i];
        *code_b = cb[j];
        return true;
      }

  return false;
}

// codes for two consecutive pixels of levels a and b with zero total disparity (a pair that is not DC balanced
// would shift the link's DC level on every pixel). Both levels may move by up to k; the smallest k wins, a moving
// less than b when possible. Host check on the palette's levels (all normal levels and masks):
// off by at most 3; over all pairs a != b: always found.
static void tmds_pair(uint8_t a, uint8_t b, uint16_t *code_a, uint16_t *code_b)
{
  uint16_t ca[4], cb[4];

  if (a == b)
  { // the usual pair: a code and its bits 0-7 inverted (balanced, at most 5 transitions, within 1 LSB)
    *code_a = tmds_encoder(a);
    *code_b = *code_a ^ 0xff;
    return;
  }

  for (int k = 0; k < 256; k++)
    for (int da = 0; da <= k; da++) // the bright level first exact, then moving
      for (int sa = 0; sa < 2; sa++)
      {
        int aa = sa ? a + da : a - da;

        if (aa < 0 || aa > 255 || (sa && da == 0))
          continue;

        int na = tmds_codes(aa, ca);
        int db_from = da == k ? 0 : k; // keep max(|da|, |db|) == k

        for (int db = db_from; db <= k; db++)
          for (int sb = 0; sb < 2; sb++)
          {
            int bb = sb ? b + db : b - db;

            if (bb < 0 || bb > 255 || (sb && db == 0))
              continue;

            if (tmds_pair_codes(ca, na, cb, tmds_codes(bb, cb), code_a, code_b))
              return;
          }
      }

  // not reached; fall back to a plain balanced pair of a
  *code_a = tmds_encoder(a);
  *code_b = *code_a ^ 0xff;
}

// one palette entry = the two TMDS pixels of a source pixel, levels first and second (B, G, R), the mask applied.
// Each channel's pair has zero total disparity; equal levels give a code and its bits 0-7 inverted.
static void set_palette_entry2(int idx, const uint8_t *first, const uint8_t *second, bool gap_row)
{
  uint8_t a[3] = {first[0], first[1], first[2]}, b[3] = {second[0], second[1], second[2]};
  uint16_t ca[3], cb[3];

  crt_mask(&crt, a, b, gap_row);

  for (int i = 0; i < 3; i++)
    if (a[i] >= b[i])
      tmds_pair(a[i], b[i], &ca[i], &cb[i]);
    else
      tmds_pair(b[i], a[i], &cb[i], &ca[i]);

  uint64_t pa = get_ser_diff_data(ca[2], ca[1], ca[0]);
  uint64_t pb = get_ser_diff_data(cb[2], cb[1], cb[0]);

  palette[idx * 4 + 0] = (uint32_t)(pa);
  palette[idx * 4 + 1] = (uint32_t)(pa >> 32);
  palette[idx * 4 + 2] = (uint32_t)(pb);
  palette[idx * 4 + 3] = (uint32_t)(pb >> 32);
}

// palette entries 0-15: the 16 colors
static void set_color_palette()
{
  uint8_t ch[3];

  for (int c = 0; c < 16; c++)
  {
    crt_color(&crt, c, ch);
    set_palette_entry2(c, ch, ch, false);
  }
}

// palette entries DIM_BASE..DIM_BASE+15: colors 0-15 as shown on scanlines
// called while DMA may read them: a pixel may get a half-updated entry for one frame, which is harmless
static void set_dim_palette(uint8_t level)
{
  uint8_t ch[3];

  for (int c = 0; c < 16; c++)
  {
    crt_color(&crt, c, ch);
    crt_dim(&crt, level, ch);
    set_palette_entry2(DIM_BASE + c, ch, ch, true);
  }
}

// palette entries BEAM_BASE.. and beam_lut: the gap color for every unordered pair of colors
static void set_beam_palette()
{
  int idx = BEAM_BASE;

  for (int a = 0; a < 16; a++)
    for (int b = a; b < 16; b++, idx++)
    {
      uint8_t la[3], lb[3], ch[3];

      crt_color(&crt, a, la);
      crt_color(&crt, b, lb);
      crt_beam(&crt, la, lb, ch);
      set_palette_entry2(idx, ch, ch, true);
      beam_lut[(a << 4) | b] = idx;
      beam_lut[(b << 4) | a] = idx;
    }
}

// all color entries (0-15, dim, BEAM) for scanline level `level` (the normal level and the mask depend on it)
// called while DMA may read them: a pixel may get a half-updated entry for one frame, which is harmless
static void set_all_palettes(uint8_t level)
{
  crt_from_settings(&crt, &settings, level);
  set_color_palette();
  set_dim_palette(level);
  set_beam_palette();
}

static void __not_in_flash_func(dma_handler_dvi)()
{
  dma_hw->ints0 = 1u << dma_ch1;

  y++;

  if (y == video_mode.whole_frame)
  {
    y = 0;
    scr_buffer = get_v_buf_out();
    active_buf_idx = 0;
  }

  if (y < video_mode.v_visible_area)
  { // visible area — render first, then set DMA to freshly rendered buffer
    if (!(y & 1))
    {
      active_buf_idx++;

      if (scr_buffer != NULL)
      {
        uint16_t scaled_y = y / video_mode.div;
        uint8_t *scr_line = &scr_buffer[scaled_y * (V_BUF_W / 2)];
        uint32_t *line_buf = v_out_dma_buf[active_buf_idx & 1];

#ifdef OSD_ENABLE
        // check if OSD is visible and overlaps with current scaled scanline
        bool osd_active = osd_state.visible && (scaled_y >= osd_mode.start_y && scaled_y < osd_mode.end_y);

        if (osd_active)
        { // calculate OSD buffer line offset using scaled coordinates (2 pixels per byte)
          uint8_t *osd_line = &osd_buffer[(scaled_y - osd_mode.start_y) * (osd_mode.width / 2)];

          int x = 0;

          if (!osd_mode.full_width)
          {
            for (; (x + 4) <= osd_mode.start_x; x += 4)
            {
              *line_buf++ = pixels[*scr_line++];
              *line_buf++ = pixels[*scr_line++];
              *line_buf++ = pixels[*scr_line++];
              *line_buf++ = pixels[*scr_line++];
            }

            for (; x < osd_mode.start_x; x++)
              *line_buf++ = pixels[*scr_line++];
          }
          else
            for (; x < osd_mode.start_x; x++)
            {
              scr_line++;
              *line_buf++ = pixels[0]; // black pixels
            }

          for (; (x + 4) <= osd_mode.end_x; x += 4)
          {
            scr_line += 4;

            *line_buf++ = pixels[*osd_line++];
            *line_buf++ = pixels[*osd_line++];
            *line_buf++ = pixels[*osd_line++];
            *line_buf++ = pixels[*osd_line++];
          }

          for (; x < osd_mode.end_x; x++)
          {
            scr_line++;
            *line_buf++ = pixels[*osd_line++];
          }

          if (!osd_mode.full_width)
          {
            for (; (x + 4) <= h_visible_area; x += 4)
            {
              *line_buf++ = pixels[*scr_line++];
              *line_buf++ = pixels[*scr_line++];
              *line_buf++ = pixels[*scr_line++];
              *line_buf++ = pixels[*scr_line++];
            }

            for (; x < h_visible_area; x++)
              *line_buf++ = pixels[*scr_line++];
          }
          else
            for (; x < h_visible_area; x++)
            {
              scr_line++;
              *line_buf++ = pixels[0]; // black pixels
            }
        }
        else
#endif
        {
          int x = 0;

          for (; (x + 4) <= h_visible_area; x += 4)
          {
            *line_buf++ = pixels[*scr_line++];
            *line_buf++ = pixels[*scr_line++];
            *line_buf++ = pixels[*scr_line++];
            *line_buf++ = pixels[*scr_line++];
          }

          for (; x < h_visible_area; x++)
            *line_buf++ = pixels[*scr_line++];
        }
      }
    }

    // odd lines repeat the even line, or show a scanline
    // (scr_buffer != NULL: the even line was rendered from the source)
    if ((y & 1) && scanlines_mode && scr_buffer != NULL)
    { // the even line's buffer is being sent now; v_out_scanline was sent two lines ago and is free
      uint32_t *dst = v_out_scanline;
      uint16_t scaled_y = y / video_mode.div;
      bool osd_line = false; // the even line has the OSD composited (entries 0-15 only)
      bool beam = scanlines_mode == SCANLINES_BEAM && (scaled_y + 1) < V_BUF_H;

#ifdef OSD_ENABLE
      osd_line = osd_state.visible && scaled_y >= osd_mode.start_y && scaled_y < osd_mode.end_y;

      if (osd_state.visible && (scaled_y + 1) >= osd_mode.start_y && scaled_y < osd_mode.end_y)
        beam = false; // this line or the next one is under the OSD
#endif

      if (beam)
      { // gap between source lines n and n + 1: pair entry per pixel (byte = 2 pixels, low nibble first, as in pixels[])
        uint8_t *l0 = &scr_buffer[scaled_y * (V_BUF_W / 2)];
        uint8_t *l1 = l0 + (V_BUF_W / 2);

        for (int x = 0; x < h_visible_area; x++)
        {
          uint32_t a = *l0++;
          uint32_t b = *l1++;
          *dst++ = IDX2(beam_lut[((a & 0x0f) << 4) | (b & 0x0f)], beam_lut[(a & 0xf0) | (b >> 4)]);
        }
      }
      else
      { // a dimmed copy of the even line: the dimmed entries sit at a fixed offset from the line's
        uint32_t *src = v_out_dma_buf[active_buf_idx & 1];

        for (int x = 0; x < h_visible_area; x++)
          *dst++ = *src++ + SCANLINE_IDX_ADD;
      }

      dma_channel_set_read_addr(dma_ch1, &v_out_scanline, false);
    }
    else
      dma_channel_set_read_addr(dma_ch1, &v_out_dma_buf[active_buf_idx & 1], false);
  }
  else if (y >= (video_mode.v_visible_area + video_mode.v_front_porch) && y < (video_mode.v_visible_area + video_mode.v_front_porch + video_mode.v_sync_pulse))
  { // V sync — use pre-filled buffer
    dma_channel_set_read_addr(dma_ch1, &v_out_sync_vsync, false);
  }
  else
  { // H blank (front/back porch) — use pre-filled buffer
    dma_channel_set_read_addr(dma_ch1, &v_out_sync_hblank, false);
  }
}

void start_dvi()
{
  int whole_line = video_mode.whole_line;

  // the setting may have changed while another output type was active
  scanlines_mode = settings.scanlines_mode;

  set_sys_clock_khz(video_mode.sys_freq, true);
  sleep_ms(10);

  // pixels[] lookup: each input byte (2 packed 4-bit pixels) → 2 palette indices
  // byte order (LSB first): p1_color, p2_color (each palette entry has norm+inv)
  // pixels[]: byte (2 packed 4-bit pixels) → 2 palette indices
  for (int i = 0; i < 256; i++)
    pixels[i] = IDX2(i & 0x0f, i >> 4);

  // TMDS control character constants (negative polarity, per DVI spec)
  const uint16_t b0 = 0b1101010100; // CTRL_00 (no sync)
  const uint16_t b1 = 0b0010101011; // CTRL_01 (V sync)
  const uint16_t b2 = 0b0101010100; // CTRL_10 (H sync)
  const uint16_t b3 = 0b1010101011; // CTRL_11 (VH sync)

  // sync palette entries (norm_lo, norm_hi, norm_lo, norm_hi — no inversion for sync)
  uint64_t sync_val;
  sync_val = get_ser_diff_data(b0, b0, b3);
  palette[NO_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[NO_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[NO_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[NO_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);
  sync_val = get_ser_diff_data(b0, b0, b2);
  palette[H_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[H_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[H_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[H_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);
  sync_val = get_ser_diff_data(b0, b0, b1);
  palette[V_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[V_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[V_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[V_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);
  sync_val = get_ser_diff_data(b0, b0, b0);
  palette[VH_SYNC * 4 + 0] = (uint32_t)(sync_val);
  palette[VH_SYNC * 4 + 1] = (uint32_t)(sync_val >> 32);
  palette[VH_SYNC * 4 + 2] = (uint32_t)(sync_val);
  palette[VH_SYNC * 4 + 3] = (uint32_t)(sync_val >> 32);

  // color entries: 16 bytes each {first pixel lo, hi, second pixel lo, hi}
  set_all_palettes(scanlines_mode);

  // set DVI data pins
  for (int i = DVI_PIN_D0; i < DVI_PIN_D0 + 6; i++)
  {
    pio_gpio_init(PIO_DVI, i);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
  }

  // set DVI clock pins
  for (int i = DVI_PIN_CLK0; i < DVI_PIN_CLK0 + 2; i++)
  {
    pio_gpio_init(PIO_DVI, i);
    gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
  }

  // allocate sync line buffers (pre-filled, never modified)
  v_out_sync_hblank = calloc(whole_line, sizeof(uint8_t));

  if (!v_out_sync_hblank)
    watchdog_reboot(0, 0, 0);

  memset((uint8_t *)v_out_sync_hblank, NO_SYNC, video_mode.h_visible_area + video_mode.h_front_porch);
  memset((uint8_t *)v_out_sync_hblank + video_mode.h_visible_area + video_mode.h_front_porch, H_SYNC, video_mode.h_sync_pulse);
  memset((uint8_t *)v_out_sync_hblank + video_mode.h_visible_area + video_mode.h_front_porch + video_mode.h_sync_pulse, NO_SYNC, video_mode.h_back_porch);

  v_out_sync_vsync = calloc(whole_line, sizeof(uint8_t));

  if (!v_out_sync_vsync)
    watchdog_reboot(0, 0, 0);

  memset((uint8_t *)v_out_sync_vsync, V_SYNC, video_mode.h_visible_area + video_mode.h_front_porch);
  memset((uint8_t *)v_out_sync_vsync + video_mode.h_visible_area + video_mode.h_front_porch, VH_SYNC, video_mode.h_sync_pulse);
  memset((uint8_t *)v_out_sync_vsync + video_mode.h_visible_area + video_mode.h_front_porch + video_mode.h_sync_pulse, V_SYNC, video_mode.h_back_porch);

  // scanline buffer: H-blank line, the visible area is rewritten on every odd line (starts black)
  v_out_scanline = malloc(whole_line);

  if (!v_out_scanline)
    watchdog_reboot(0, 0, 0);

  memcpy((uint8_t *)v_out_scanline, (uint8_t *)v_out_sync_hblank, whole_line);
  memset((uint8_t *)v_out_scanline, 0, video_mode.h_visible_area);

  // Ping-pong image line buffers as a single contiguous block
  v_out_dma_buf_alloc = calloc(whole_line * 2, sizeof(uint8_t));

  if (!v_out_dma_buf_alloc)
    watchdog_reboot(0, 0, 0);

  v_out_dma_buf[0] = v_out_dma_buf_alloc;
  v_out_dma_buf[1] = (uint32_t *)((uint8_t *)v_out_dma_buf_alloc + whole_line);
  memcpy((uint8_t *)v_out_dma_buf[0], (uint8_t *)v_out_sync_hblank, whole_line);
  memcpy((uint8_t *)v_out_dma_buf[1], (uint8_t *)v_out_sync_hblank, whole_line);

  // === Output PIO (SM0): TMDS serializer ===
  pio_sm_config c = pio_get_default_sm_config();

  offset = pio_add_program(PIO_DVI, &pio_dvi_program);
  sm_config_set_wrap(&c, offset, offset + pio_dvi_program.length - 1);

  sm_config_set_out_pins(&c, DVI_PIN_D0, 6);
  pio_sm_set_consecutive_pindirs(PIO_DVI, SM_DVI, DVI_PIN_D0, 6, true);

  pio_sm_set_pins_with_mask(PIO_DVI, SM_DVI, 3u << DVI_PIN_CLK0, 3u << DVI_PIN_CLK0);
  pio_sm_set_pindirs_with_mask(PIO_DVI, SM_DVI, 3u << DVI_PIN_CLK0, 3u << DVI_PIN_CLK0);

  sm_config_set_sideset_pins(&c, DVI_PIN_CLK0);
  sm_config_set_sideset(&c, 2, false, false);

  sm_config_set_out_shift(&c, true, true, 30);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  pio_sm_init(PIO_DVI, SM_DVI, offset, &c);
  pio_sm_set_enabled(PIO_DVI, SM_DVI, true);

  // === Conv PIO (SM1): index → palette address converter ===
  pio_sm_config c_conv = pio_get_default_sm_config();

  offset_conv = pio_add_program(PIO_DVI, &pio_dvi_conv_program);
  sm_config_set_wrap(&c_conv, offset_conv, offset_conv + pio_dvi_conv_program.length - 1);
  sm_config_set_in_shift(&c_conv, true, false, 32);  // shift right, no autopush
  sm_config_set_out_shift(&c_conv, true, false, 32); // shift right, no autopull (explicit pull used)

  pio_sm_clear_fifos(PIO_DVI, SM_DVI_CONV);
  pio_sm_restart(PIO_DVI, SM_DVI_CONV);

  // load palette base address >> 12 into X register (4KB-aligned, 16-byte entries)
  pio_set_x(PIO_DVI, SM_DVI_CONV, ((uint32_t)palette) >> 12);

  pio_sm_init(PIO_DVI, SM_DVI_CONV, offset_conv, &c_conv);
  pio_sm_set_enabled(PIO_DVI, SM_DVI_CONV, true);

  // === DMA initialization (4 channels) ===
  dma_ch0 = dma_claim_unused_channel(true);
  dma_ch1 = dma_claim_unused_channel(true);
  dma_ch2 = dma_claim_unused_channel(true);
  dma_ch3 = dma_claim_unused_channel(true);

  // ch0: data line → conv PIO TX (feeds index buffer to converter)
  dma_channel_config c0 = dma_channel_get_default_config(dma_ch0);

  channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
  channel_config_set_read_increment(&c0, true);
  channel_config_set_write_increment(&c0, false);
  channel_config_set_dreq(&c0, DREQ_PIO_DVI + SM_DVI_CONV); // conv SM TX
  channel_config_set_chain_to(&c0, dma_ch1);

  dma_channel_configure(
      dma_ch0,
      &c0,
      &PIO_DVI->txf[SM_DVI_CONV], // write: conv PIO TX FIFO
      &v_out_dma_buf[0][0],       // read: index buffer
      whole_line / 4,             // transfer count: bytes / 4
      false                       // don't start yet
  );

  // ch1: control — reloads ch0 read addr, fires IRQ
  dma_channel_config c1 = dma_channel_get_default_config(dma_ch1);

  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
  channel_config_set_read_increment(&c1, false);
  channel_config_set_write_increment(&c1, false);
  channel_config_set_chain_to(&c1, dma_ch0);

  dma_channel_configure(
      dma_ch1,
      &c1,
      &dma_hw->ch[dma_ch0].read_addr, // write: ch0's read addr
      &v_out_dma_buf[0],              // read: pointer to buffer
      1,
      false // don't start yet
  );

  // ch2: out_data — reads palette entry → sends TMDS data to output PIO
  // NOTE: ch2 MUST be configured before ch3, because ch3 chains to ch2
  dma_channel_config c2 = dma_channel_get_default_config(dma_ch2);
  channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
  channel_config_set_read_increment(&c2, true);
  channel_config_set_write_increment(&c2, false);
  channel_config_set_dreq(&c2, DREQ_PIO_DVI + SM_DVI); // output SM TX
  channel_config_set_chain_to(&c2, dma_ch3);

  dma_channel_configure(
      dma_ch2,
      &c2,
      &PIO_DVI->txf[SM_DVI], // write: output PIO TX FIFO
      palette,               // read: palette (overwritten by ch3 each cycle)
      4,                     // 4 × uint32_t = 16 bytes per palette entry (norm + inv)
      false                  // don't start yet
  );

  // ch3: set_addr — reads palette address from conv RX → sets ch2 read addr
  dma_channel_config c3 = dma_channel_get_default_config(dma_ch3);
  channel_config_set_transfer_data_size(&c3, DMA_SIZE_32);
  channel_config_set_read_increment(&c3, false);
  channel_config_set_write_increment(&c3, false);
  channel_config_set_dreq(&c3, DREQ_PIO0_RX0 + SM_DVI_CONV); // conv SM RX
  channel_config_set_chain_to(&c3, dma_ch2);

  dma_channel_configure(
      dma_ch3,
      &c3,
      &dma_hw->ch[dma_ch2].read_addr, // write: ch2's read addr
      &PIO_DVI->rxf[SM_DVI_CONV],     // read: conv RX FIFO
      1,
      true // start immediately — ch2 is already configured
  );

  // IRQ setup
  dma_channel_set_irq0_enabled(dma_ch1, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_dvi);
  irq_set_priority(DMA_IRQ_0, PICO_HIGHEST_IRQ_PRIORITY);
  irq_set_enabled(DMA_IRQ_0, true);

  bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
  // start the line DMA (ch3↔ch2 loop is already running)
  dma_channel_start(dma_ch0);
}

void stop_dvi()
{
  // disable IRQ first to prevent handlers from running during cleanup
  irq_set_enabled(DMA_IRQ_0, false);
  irq_remove_handler(DMA_IRQ_0, dma_handler_dvi);

  // reset ISR state for clean restart
  y = 0;
  scr_buffer = NULL;
  active_buf_idx = 0;

  // stop output PIO (SM0)
  pio_sm_set_enabled(PIO_DVI, SM_DVI, false);
  pio_sm_init(PIO_DVI, SM_DVI, offset, NULL);
  pio_remove_program(PIO_DVI, &pio_dvi_program, offset);

  // stop conv PIO (SM1)
  pio_sm_set_enabled(PIO_DVI, SM_DVI_CONV, false);
  pio_sm_init(PIO_DVI, SM_DVI_CONV, offset_conv, NULL);
  pio_remove_program(PIO_DVI, &pio_dvi_conv_program, offset_conv);

  // cleanup and free all 4 DMA channels
  dma_channel_cleanup(dma_ch0);
  dma_channel_cleanup(dma_ch1);
  dma_channel_cleanup(dma_ch2);
  dma_channel_cleanup(dma_ch3);
  dma_channel_unclaim(dma_ch0);
  dma_channel_unclaim(dma_ch1);
  dma_channel_unclaim(dma_ch2);
  dma_channel_unclaim(dma_ch3);

  // free index buffers (single contiguous allocation)
  if (v_out_dma_buf_alloc != NULL)
  {
    free(v_out_dma_buf_alloc);
    v_out_dma_buf_alloc = NULL;
  }

  v_out_dma_buf[0] = NULL;
  v_out_dma_buf[1] = NULL;

  // free sync buffers
  if (v_out_sync_hblank != NULL)
  {
    free(v_out_sync_hblank);
    v_out_sync_hblank = NULL;
  }

  if (v_out_sync_vsync != NULL)
  {
    free(v_out_sync_vsync);
    v_out_sync_vsync = NULL;
  }

  if (v_out_scanline != NULL)
  {
    free(v_out_scanline);
    v_out_scanline = NULL;
  }
}

// the CRT effect settings (beam width, mask, normal level) changed
void set_dvi_crt()
{
  set_all_palettes(scanlines_mode);
}

void set_dvi_scanlines_mode(uint8_t level)
{
  // before enabling, so the first scanline already uses the new level; the normal level and the mask change with
  // scanlines on / off. Not before start_dvi (crt.beam_gap unset), which builds all entries itself.
  if (crt.beam_gap)
    set_all_palettes(level);

  scanlines_mode = level;
}
