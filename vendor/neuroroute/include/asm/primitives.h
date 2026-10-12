#ifndef NEURO_ASM_PRIMITIVES_H
#define NEURO_ASM_PRIMITIVES_H

#include <stdatomic.h>

/*
 * Portable C11 spin lock used for topology changes.
 * The routing counter itself still uses the original GCC/Clang atomic builtin.
 */
typedef struct {
    atomic_flag flag;
} spinlock_t;

#define SPINLOCK_INITIALIZER { ATOMIC_FLAG_INIT }

static inline void spinlock_init(spinlock_t *lock) {
    if (lock != NULL) {
        atomic_flag_clear_explicit(&lock->flag, memory_order_release);
    }
}

static inline void spinlock_lock(spinlock_t *lock) {
    while (atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire)) {
        /* Busy wait: appropriate only for very short critical sections. */
    }
}

static inline void spinlock_unlock(spinlock_t *lock) {
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
}

#endif
