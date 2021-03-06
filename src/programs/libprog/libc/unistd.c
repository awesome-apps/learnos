#include <unistd.h>
#include <stdbool.h>
#include <anlock.h>
#include <base/alloc.h>
#include <base/system.h>

static intptr_t brkSize __attribute__((aligned(8))) = 0;
static uint64_t brkLock __attribute__((aligned(8))) = 0;

static void * _sbrk_no_lock(intptr_t amount);
static int _gain_memory(uintptr_t amount);
static int _lose_memory(uintptr_t amount);

int brk(const void * addr) {
  anlock_lock(&brkLock);
  intptr_t len = (intptr_t)(addr - ALLOC_DATA_BASE);
  intptr_t change = len - brkSize;
  void * res = _sbrk_no_lock(change);
  anlock_unlock(&brkLock);
  return res == (void *)-1 ? -1 : 0;
}

void * sbrk(intptr_t increment) {
  anlock_lock(&brkLock);
  void * res = _sbrk_no_lock(increment);
  anlock_unlock(&brkLock);
  return res;
}

void _exit(int unused) {
  sys_exit();
}

unsigned int sleep(unsigned int secs) {
  uint64_t start = sys_get_time();
  // sleep and then return the amount of time we missed
  sys_clear_unsleep();
  sys_sleep(secs * 1000000);
  uint64_t delayed = sys_get_time() - start;
  return delayed / 1000000;
}

int usleep(useconds_t time) {
  sys_clear_unsleep();
  sys_sleep(time);
  return 0;
}

static void * _sbrk_no_lock(intptr_t increment) {
  if (increment < -brkSize) {
    return (void *)(-1);
  }
  void * base = ALLOC_DATA_BASE + brkSize;
  if (increment < 0) {
    if (_lose_memory((uintptr_t)-increment)) {
      return (void *)(-1);
    }
  } else {
    if (_gain_memory((uintptr_t)increment)) {
      return (void *)(-1);
    }
  }
  return base;
}

static int _gain_memory(uintptr_t amount) {
  uint64_t firstByte = brkSize;
  uint64_t lastByte = brkSize + amount;
  uint64_t firstPage = (firstByte >> 12) + (firstByte & 0xfff ? 1 : 0);
  uint64_t lastPage = (lastByte >> 12) + (lastByte & 0xfff ? 1 : 0);
  if (lastPage <= firstPage) return 0;

  uint64_t count = lastPage - firstPage;
  if (!alloc_pages(firstPage, count)) return -1;

  brkSize += amount;
  return 0;
}

static int _lose_memory(uintptr_t amount) {
  uint64_t firstByte = brkSize - amount;
  uint64_t lastByte = brkSize;
  uint64_t firstPage = (firstByte >> 12) + (firstByte & 0xfff ? 1 : 0);
  uint64_t lastPage = (lastByte >> 12) + (lastByte & 0xfff ? 1 : 0);
  if (lastPage <= firstPage) return 0;

  uint64_t count = lastPage - firstPage;
  if (!free_pages(firstPage, count)) return -1;
  brkSize -= amount;
  return 0;
}

