#include <am.h>
#include <nemu.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {

  outl(SYNC_ADDR, 1); // 1代表需要刷屏幕
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  int h = (uint16_t)inw(VGACTL_ADDR);  // 低16位是h, 高16位是w
  int w = (uint16_t)inw(VGACTL_ADDR + sizeof(uint16_t)); // 或 inl(VGACTL_ADDR) >> 16
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = w, .height = h,
    .vmemsz = 0
  };
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  // int x, y; void *pixels; int w, h; bool sync
  // 00RRGGBB
  int x = ctl->x;
  int y = ctl->y;
  int w = ctl->w;
  int h = ctl->h;
  uint32_t *pixels = ctl->pixels;

  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  uint32_t screen_w = inl(VGACTL_ADDR) >> 16;
  int block_i = 0;
  for (int i = y; i < y + h; i++) {
    for (int j = x; j < x + w; j++) {
      fb[screen_w * i + j] = pixels[block_i];
      block_i++;
    }
  }

  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = (bool)inl(SYNC_ADDR);
}
