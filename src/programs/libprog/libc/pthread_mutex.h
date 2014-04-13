#ifndef __PTHREAD_MUTEX_H__
#define __PTHREAD_MUTEX_H__

#include <lock.h>
#include <stdint.h>
#include <stdbool.h>

#define PTHREAD_MUTEX_DEFAULT 0
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_INITIALIZER {BASIC_LOCK_INITIALIZER, 0, 0, 0, 0}

struct pthread_mutex {
  basic_lock_t lock; // seize this to serialize holding of the lock

  // fields contained by contentLock
  uint64_t contentLock;
  uint64_t holdingThread; // thread ID holding the lock
  uint64_t holdingCount; // number of levels of lock (0 = unlocked)
  uint64_t type;
} __attribute__((packed));

struct pthread_mutexattr {
  int64_t type;
} __attribute__((packed));

typedef struct pthread_mutex pthread_mutex_t;
typedef struct pthread_mutexattr pthread_mutexattr_t;

int pthread_mutexattr_init(pthread_mutexattr_t * attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t * attr);
int pthread_mutexattr_gettype(const pthread_mutexattr_t * attr, int * type);
int pthread_mutexattr_settype(pthread_mutexattr_t * attr, int type);

void pthread_mutex_init(pthread_mutex_t * mutex,
                        const pthread_mutexattr_t * attr);

#endif
