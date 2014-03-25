/**
 * Equivalent to this:
 *********************
 * cli
 * push rbp
 * mov rbp, rsp
 * mov rsp, NEW_STACK
 * jmp [KERN_ADDR]
 ***************************/
typedef struct {
  char code1[7]; // FA 55 48 89 E5 48 BC
  uint64_t stack;
  char code2[3]; // FF 24 25
  uint32_t kernCall;
} __attribute__((packed)) syscall_code;

typedef struct {
  uint64_t rsp;
  uint64_t rbp;
  uint64_t cr3;
  uint64_t rip;
  uint64_t flags;
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;

  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;

  uint64_t cs;
  uint64_t ss;

  syscall_code callCode;
} __attribute__((packed)) anscheduler_state;

typedef struct {
  uint64_t reserved;
  // TODO: see if i can declare an empty struct
} __attribute__((packed)) anscheduler_task_ui_t;

