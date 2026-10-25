#include <stdint.h>
#include <stdatomic.h>

typedef struct {
    _Atomic uint32_t flags;
} EventFlags;

void event_flags_init(EventFlags* e) {
    atomic_init(&e->flags, 0u);
}

void event_flags_set(EventFlags* e, uint32_t mask) {
    // Your code here
}

uint32_t event_flags_take(EventFlags* e) {
    // Your code here
    return 0;
}