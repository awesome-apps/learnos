#include "kernpage.h"
#include <stdio.h>
#include <libkern_base.h>
#include <shared/addresses.h>
#include <anmem/alloc.h>
#include <anmem/config.h>

static uint64_t phyMapCount = 0;
static anmem_t anmemRoot;
static uint64_t usedPages = 0;

static void _kernpage_get_regions();
static void _kernpage_make_mapping();
static void _kernpage_configure_anmem();

// old kernpage functions
static page_t _kernpage_next_physical();
static page_t _kernpage_allocate_physical();
static bool _kernpage_lin_map(page_t virt, page_t phys);

// search functions
static bool _kernpage_find_physical(uint64_t * table,
                                    page_t page,
                                    uint8_t depth,
                                    page_t base,
                                    page_t * virt);

void kernpage_initialize() {
  _kernpage_get_regions();
  _kernpage_make_mapping();
  
  print("last pages: virtual=0x");
  printHex((uint64_t)LAST_VPAGE);
  print(" physical=0x");
  printHex((uint64_t)LAST_PAGE);
  print("\n");
  
  _kernpage_configure_anmem();
}

page_t kernpage_calculate_virtual(page_t phys) {
  int i;
  const kernpage_info * maps = (const kernpage_info *)PHYSICAL_MAP_ADDR;
  uint64_t virtual = 0;
  for (i = 0; i < phyMapCount; i++) {
    if (maps[i].start > phys) return 0; // we've gone too far
    if (maps[i].start <= phys && maps[i].start + maps[i].length > phys) {
      return virtual + phys - maps[i].start;
    }
    virtual += maps[i].length;
  }
  return 0;
}

page_t kernpage_calculate_physical(page_t virt) {
  int i;
  const kernpage_info * maps = (const kernpage_info *)PHYSICAL_MAP_ADDR;
  for (i = 0; i < phyMapCount; i++) {
    if (maps[i].start + maps[i].length > virt) {
      return maps[i].start + virt;
    }
    virt -= maps[i].length;
  }
  return 0;
}

uint64_t kernpage_last_virtual() {
  return LAST_VPAGE;
}

/**
 * Returns `true` if a virtual page is mapped, `false` if not.
 */
bool kernpage_is_mapped(page_t page) {
  if (page > LAST_VPAGE) return false;
  int i;

  // check if it's set in the page table
  uint64_t indexInPT = page % 0x200;
  uint64_t indexInPDT = (page >> 9) % 0x200;
  uint64_t indexInPDPT = (page >> 18) % 0x200;
  uint64_t indexInPML4 = (page >> 27) % 0x200;
  uint64_t indices[4] = {indexInPML4, indexInPDPT, indexInPDT, indexInPT};
  uint64_t * tablePtr = (uint64_t *)PML4_START;
  for (i = 0; i < 4; i++) {
    uint64_t value = tablePtr[indices[i]];
    if (!(value & 0x03)) {
      return false;
    }
    uint64_t physPage = value >> 12;
    uint64_t virPage = kernpage_calculate_virtual(physPage);
    tablePtr = (uint64_t *)(virPage << 12);
  }
  return true;
}

bool kernpage_map(page_t virt, page_t phys) {
  // very similar to _kernpage_lin_map() but uses anmem.
  
  int i;
  uint64_t indexInPT = virt % 0x200;
  uint64_t indexInPDT = (virt >> 9) % 0x200;
  uint64_t indexInPDPT = (virt >> 18) % 0x200;
  uint64_t indexInPML4 = (virt >> 27) % 0x200;
  uint64_t indices[4] = {indexInPML4, indexInPDPT, indexInPDT, indexInPT};
  volatile uint64_t * tablePtr = (uint64_t *)PML4_START;
  
  for (i = 0; i < 3; i++) {
    uint64_t value = tablePtr[indices[i]];
    if (!(value & 0x03)) {
      // create a subtable
      uint64_t newVirPage = kernpage_alloc_virtual();
      if (!newVirPage) return false;
      uint64_t newPage = kernpage_calculate_physical(newVirPage);
      if (!newPage) return false;
      
      volatile uint64_t * newData = (uint64_t *)(newVirPage << 12);
      int j;
      for (j = 0; j < 0x200; j++) newData[j] = 0;
      tablePtr[indices[i]] = (newPage << 12) | 3;
      tablePtr = newData;
    } else {
      uint64_t physPage = value >> 12;
      uint64_t virPage = kernpage_calculate_virtual(physPage);
      if (!virPage) return false;
      tablePtr = (uint64_t *)(virPage << 12);
    }
  }
  
  tablePtr[indices[3]] = (phys << 12) | 3;
  invalidate_page(virt);
  if (virt > LAST_VPAGE) LAST_VPAGE = virt;
  return true;
}

bool kernpage_lookup_virtual(page_t phys, page_t * virt) {
  if (phys == 0) {
    *virt = 0;
    return true;
  }
  
  uint64_t quickResult = kernpage_calculate_virtual(phys);
  if (quickResult) return quickResult;

  uint64_t * tablePtr = (uint64_t *)PML4_START;
  return _kernpage_find_physical(tablePtr, phys, 3, 0, virt);
}

page_t kernpage_alloc_virtual() {
  void * buffer = anmem_alloc_page(&anmemRoot);
  if (!buffer) return 0;
  __sync_fetch_and_add(&usedPages, 1);
  return ((uint64_t)buffer) >> 12;
}

void kernpage_free_virtual(page_t virt) {
  __sync_fetch_and_sub(&usedPages, 1);
  anmem_free_page(&anmemRoot, (void *)(virt << 12));
}

uint64_t kernpage_alloc_pci(uint64_t pageCount) {
  void * buffer = anmem_alloc_aligned(&anmemRoot, pageCount);
  if (!buffer) return 0;
  __sync_fetch_and_add(&usedPages, pageCount);
  uint64_t page = ((uint64_t)buffer) >> 12;
  return kernpage_calculate_physical(page) << 12;
}

void kernpage_free_pci(uint64_t addr, uint64_t pageCount) {
  uint64_t vir = kernpage_calculate_virtual(addr >> 12) << 12;
  anmem_free_aligned(&anmemRoot, (void *)vir, pageCount);
  __sync_fetch_and_sub(&usedPages, pageCount);
}

uint64_t kernpage_count_allocated() {
  return usedPages; // atomic if it's aligned
}

void kernpage_copy_physical(void * _dest,
                            const void * physical,
                            uint64_t len) {
  if (len == 0) return;
  unsigned char * dest = _dest;
  uint64_t pageOffset = ((uint64_t)physical) & 0xfff;

  // make sure each page is mapped
  uint64_t minPage = ((uint64_t)physical) >> 12;
  uint64_t maxPage = ((uint64_t)physical + len - 1) >> 12;
  uint64_t curPage;
  for (curPage = minPage; curPage <= maxPage; curPage++) {
    uint64_t virPage;
    if (!kernpage_lookup_virtual(curPage, &virPage)) {
      virPage = kernpage_last_virtual() + 1;
      if (!kernpage_map(virPage, curPage)) {
        die("Failed to map page for kernpage_copy_physical");
      }
    }
    unsigned char * virPtr = (unsigned char *)(virPage << 12);
    unsigned char * destPtr = NULL;
    uint64_t copyLen = 0, i;
    if (curPage == minPage) {
      copyLen = len;
      if (copyLen > 0x1000 - pageOffset) {
        copyLen = 0x1000 - pageOffset;
      }
      virPtr = &virPtr[pageOffset];
      destPtr = dest;
    } else if (curPage == maxPage) {
      copyLen = (len + pageOffset) & 0xfff;
      destPtr = &dest[len - copyLen];
    } else {
      // copy the entire page
      destPtr = &dest[((curPage - minPage) << 12) - pageOffset];
      copyLen = 0x1000;
    }
    for (i = 0; i < copyLen; i++) destPtr[i] = virPtr[i];
  }
}

/***********
 * Private *
 ***********/

static void _kernpage_get_regions() {
  uint32_t mmapLength = MBOOT_INFO[11];
  uint64_t mmapAddr = (uint64_t)MBOOT_INFO[12];

  if ((uint64_t)mmapAddr >= 0x100000) {
    die("maps expected in lower-memory");
  }

  // By this point, startup/page_init.c has already given us access
  // to enough physical memory that we should definitely be able
  // to access the mmap structure.
  const mmap_info * mmap = NULL;
  const mmap_info * next = (const mmap_info *)((uint64_t)mmapAddr);
  volatile kernpage_info * destMap = (kernpage_info *)PHYSICAL_MAP_ADDR;

  int count = 0, i;
  while (mmapLength > 0 && count < 255) {
    // iterating maps
    mmap = next;
    next = (const mmap_info *)((const char *)mmap + mmap->size + 4);
    mmapLength -= mmap->size + 4;
    if (mmap->type != 1) continue;

    // This list will not be perfectly 1-to-1 because we identity map
    // some lower memory from page_init.c, so here we need to make sure
    // we respect that
    if (mmap->base_addr + mmap->length <= 0x100000) continue;

    // make sure that the memory is page aligned
    uint64_t start = mmap->base_addr;
    uint64_t len = mmap->length;
    if (start & 0xfff) {
      // push up the memory to the next
      uint64_t shard = 0x1000 - (start & 0xfff);
      if (len <= shard) continue;
      start += shard;
      len -= shard;
    }
    len >>= 12;
    start >>= 12;

    if (start <= 0x100) {
      // the first physical segment must map the lower 1MB at least
      len += start;
      start = 0;
    }
    kernpage_info info = {start, len};
    print("adding info {");
    printHex(info.start);
    print(", ");
    printHex(info.length);
    print("}\n");

    // insert the kernpage_info where it fits
    int insIndex = count;
    for (i = 0; i < count; i++) {
      if (destMap[i].start > start) {
        insIndex = i;
        break;
      }
    }
    for (i = count; i > insIndex; i--) {
      destMap[i] = destMap[i - 1];
    }
    destMap[insIndex] = info;
    count++;
  }
  if (count < 1) {
    die("no immediate upper memory");
  }
  phyMapCount = (uint8_t)count;
  print("There are ");
  printHex(count);
  print(" memory maps: ");
  for (i = 0; i < count; i++) {
    if (i != 0) print(", ");
    print("0x");
    printHex(destMap[i].start);
    print(":");
    printHex(destMap[i].length);
  }
  print("\n");
}

static void _kernpage_make_mapping() {
  print("expanding physical map...\n");
  // map pages
  int i;
  uint64_t j;
  const kernpage_info * maps = (const kernpage_info *)PHYSICAL_MAP_ADDR;
  uint64_t virtual = 0, created = 0;
  print("last page (initial) = ");
  printHex(LAST_PAGE);
  print(", vpage = ");
  printHex(LAST_VPAGE);
  print("\n");
  for (i = 0; i < phyMapCount; i++) {
    for (j = maps[i].start; j < maps[i].length + maps[i].start; j++) {
      if (!kernpage_is_mapped(virtual)) {
        _kernpage_lin_map(virtual, j);
        created++;
      }
      virtual++;
    }
  }
  print("mapped 0x");
  printHex(created);
  print(" new pages to virtual memory\n");
}

static void _kernpage_configure_anmem() {
  // initialize the structure
  uint64_t firstVpage = kernpage_calculate_virtual(LAST_PAGE + 1);

  kernpage_info * maps = (kernpage_info *)PHYSICAL_MAP_ADDR;

  anmemRoot.allocators = (anmem_section_t *)ANMEM_SECTIONS;
  anmemRoot.maximum = 0x1000 / sizeof(anmem_section_t);
  anmemRoot.count = 0;

  anmem_config_t config;
  config.structs = maps;
  config.sizeOffset = 8;
  config.physPageOffset = 0;
  config.structSize = 0x10;
  config.structCount = phyMapCount;

  // 2^15 * 0x1000 = 128MiB
  bool result = anmem_configure(&config, &anmemRoot, 15, firstVpage);
  if (!result) die("anmem_configure() failed");

  result = anmem_init_structures(&anmemRoot);
  if (!result) die("anmem_init_structures() failed");

  print("anmem configured, skip=0x");
  printHex(firstVpage);
  print(", sections=0x");
  printHex(anmemRoot.count);
  print("\n");

  LAST_PAGE = 0;
}

/************************
 * Old kernpage methods *
 ************************/

static page_t _kernpage_next_physical() {
  uint64_t page = LAST_PAGE;
  int i;
  const kernpage_info * maps = (const kernpage_info *)PHYSICAL_MAP_ADDR;
  for (i = 0; i < phyMapCount; i++) {
    kernpage_info map = maps[i];
    if (map.start + map.length > page && map.start <= page) {
      if (page < map.start + map.length - 1) {
        return page + 1;
      } else if (i == phyMapCount - 1) {
        return 0;
      } else {
        return maps[i + 1].start;
      }
    }
  }
  return 0;
}

static page_t _kernpage_allocate_physical() {
  uint64_t next = _kernpage_next_physical();
  if (!next) return 0;
  return (LAST_PAGE = next);
}

static bool _kernpage_lin_map(page_t virt, page_t phys) {
  int i;
  uint64_t indexInPT = virt % 0x200;
  uint64_t indexInPDT = (virt >> 9) % 0x200;
  uint64_t indexInPDPT = (virt >> 18) % 0x200;
  uint64_t indexInPML4 = (virt >> 27) % 0x200;
  uint64_t indices[4] = {indexInPML4, indexInPDPT, indexInPDT, indexInPT};
  volatile uint64_t * tablePtr = (uint64_t *)PML4_START;
  for (i = 0; i < 3; i++) {
    uint64_t value = tablePtr[indices[i]];
    if (!(value & 0x03)) {
      // create a subtable
      uint64_t newPage = _kernpage_allocate_physical();
      if (!newPage) return false;
      uint64_t newVirPage = kernpage_calculate_virtual(newPage);
      if (!newVirPage) return false;
      volatile uint64_t * newData = (uint64_t *)(newVirPage << 12);
      int j;
      for (j = 0; j < 0x200; j++) newData[j] = 0;
      tablePtr[indices[i]] = (newPage << 12) | 3;
      tablePtr = newData;
    } else {
      uint64_t physPage = value >> 12;
      uint64_t virPage = kernpage_calculate_virtual(physPage);
      if (!virPage) return false;
      tablePtr = (uint64_t *)(virPage << 12);
    }
  }
  tablePtr[indices[3]] = (phys << 12) | 3;
  invalidate_page(virt);
  if (virt > LAST_VPAGE) LAST_VPAGE = virt;
  return true;
}

/**********************************
 * Searching (for love--and pages) *
 **********************************/

static bool _kernpage_find_physical(uint64_t * table,
                                    uint64_t page,
                                    uint8_t depth,
                                    page_t base,
                                    page_t * virt) {
  int i;
  if (depth == 0) {
    for (i = 0x1ff; i >= 0; i--) {
      if (!(table[i] & 0x3)) continue;
      if (table[i] >> 12 == page) {
        *virt = base + i;
        return true;
      }
    }
    return false;
  }

  // recursive search
  uint64_t eachSize = 1 << (9 * depth);
  for (i = 0x1ff; i >= 0; i--) {
    if (!(table[i] & 0x3)) continue;
    uint64_t virPage = kernpage_calculate_virtual(table[i] >> 12);
    uint64_t * nextTable = (uint64_t *)(virPage << 12);
    bool result = _kernpage_find_physical(nextTable,
                                          page,
                                          depth - 1,
                                          base + (eachSize * i),
                                          virt);
    if (result) return true;
  }
  return false;
}
