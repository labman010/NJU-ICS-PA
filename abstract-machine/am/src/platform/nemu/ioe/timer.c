#include <am.h>
#include <nemu.h>

void __am_timer_init() {
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  // 这里必须先读高32位,再读低32位,否则时间将不会刷新,详见 rtc_io_handler 的 offset+4 的判断
  uptime->us = (uint64_t)inl(RTC_ADDR + 0x4) << 32 | (uint64_t)inl(RTC_ADDR);
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
