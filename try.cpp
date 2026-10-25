#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

class MemoryPool {
    struct Node {
        Node* next;
    };

    std::byte* arena;
    Node* freeList;
    std::size_t blockSize;
    std::size_t capacity;
    std::size_t freeCount;
    std::unique_ptr<bool[]> allocatedBlocks;

    bool blockIndex(const void* p, std::size_t& index) const noexcept {
        const auto base = reinterpret_cast<std::uintptr_t>(arena);
        const auto address = reinterpret_cast<std::uintptr_t>(p);
        if (address < base) return false;

        const std::uintptr_t offset = address - base;
        const std::size_t arenaSize = capacity * blockSize;
        if (offset >= arenaSize || offset % blockSize != 0) return false;

        index = static_cast<std::size_t>(offset / blockSize);
        return true;
    }

public:
    enum class FreeResult {
        success,
        null_pointer,
        invalid_pointer,
        already_free
    };

    MemoryPool(std::size_t cap, std::size_t bsz)
        : arena(nullptr), freeList(nullptr), blockSize(0), capacity(cap), freeCount(0),
          allocatedBlocks(nullptr) {
        constexpr std::size_t alignment = alignof(std::max_align_t);
        const std::size_t minimumSize = bsz < sizeof(Node) ? sizeof(Node) : bsz;
        const std::size_t maxSize = std::numeric_limits<std::size_t>::max();

        if (minimumSize > maxSize - (alignment - 1)) {
#if defined(__cpp_exceptions)
            throw std::bad_array_new_length();
#else
            std::abort();
#endif
        }
        blockSize = ((minimumSize + alignment - 1) / alignment) * alignment;

        if (cap == 0) return;
        if (cap > maxSize / blockSize) {
#if defined(__cpp_exceptions)
            throw std::bad_array_new_length();
#else
            std::abort();
#endif
        }

        allocatedBlocks.reset(new bool[cap]{});
        arena = static_cast<std::byte*>(
            ::operator new(cap * blockSize, std::align_val_t{alignment}));
        for (std::size_t i = 0; i < cap; ++i) {
            auto* node = ::new (static_cast<void*>(arena + i * blockSize)) Node{freeList};
            freeList = node;
            ++freeCount;
        }
    }

    ~MemoryPool() {
        ::operator delete(arena, std::align_val_t{alignof(std::max_align_t)});
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    void* alloc() {
        if (freeList == nullptr) return nullptr;

        Node* freeBlock = freeList;
        std::size_t index = 0;
        if (!blockIndex(freeBlock, index)) {
#if defined(__cpp_exceptions)
            throw std::logic_error("memory pool free list is corrupted");
#else
            std::abort();
#endif
        }
        freeList = freeList->next;
        allocatedBlocks[index] = true;
        --freeCount;
        return freeBlock;
    }

    // Destroy any object in the block before returning its storage to the pool.
    // Invalid non-null pointers and double frees throw std::invalid_argument when
    // exceptions are enabled; otherwise, inspect the returned FreeResult.
    FreeResult free(void* p) {
        const FreeResult result = tryFree(p);
        if (result == FreeResult::success || result == FreeResult::null_pointer) return result;
#if defined(__cpp_exceptions)
        throw std::invalid_argument(
            result == FreeResult::already_free
                ? "memory pool block was already freed"
                : "pointer is not the start of a block in this memory pool");
#else
        return result;
#endif
    }

    // Use this status-returning variant when the caller handles errors without exceptions.
    FreeResult tryFree(void* p) noexcept {
        if (p == nullptr) return FreeResult::null_pointer;

        std::size_t index = 0;
        if (!blockIndex(p, index)) return FreeResult::invalid_pointer;
        if (!allocatedBlocks[index]) {
            return FreeResult::already_free;
        }
        auto* block = static_cast<Node*>(p);
        ::new (static_cast<void*>(block)) Node{freeList};
        freeList = block;
        allocatedBlocks[index] = false;
        ++freeCount;
        return FreeResult::success;
    }

    std::size_t available() const { return freeCount; }
};

int main() {
    MemoryPool buffer(5, 32);
    int* ptr_arr[5];
    std::size_t count = 0;

    while (count < 5) {
        void* memory = buffer.alloc();
        if (memory == nullptr) break;

        ptr_arr[count] = ::new (memory) int(static_cast<int>(count + 1));
        ++count;
    }

    for (std::size_t i = 0; i < count; ++i) {
        std::cout << ptr_arr[i] << ": " << *ptr_arr[i] << '\n';
    }

    for (std::size_t i = 0; i < count; ++i) {
        buffer.free(ptr_arr[i]);
    }

    std::cout << "Available after freeing: " << buffer.available() << '\n';
    return 0;
}
