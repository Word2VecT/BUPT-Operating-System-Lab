/**
 * @file peterson_algorithm_physical_address.cpp
 * @brief Demonstrates Peterson's Algorithm for mutual exclusion while verifying the physical address of a shared variable.
 *
 * This code uses the Peterson Algorithm to manage mutual exclusion between two processes (parent and child).
 * Additionally, it attempts to map the logical (virtual) address of a shared variable to its physical address
 * and verify that the value read from /dev/mem matches the logical value. This example requires privileges
 * to open /dev/mem and is system-dependent (Linux-based systems providing /proc/self/pagemap).
 *
 * Usage:
 *   Compile and run. The program creates a child process via fork() and both processes execute
 *   Peterson's Algorithm in parallel, using shared memory for synchronization.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

/**
 * @brief Converts a virtual address to a physical address by reading /proc/self/pagemap.
 *
 * This function reads the pagemap entry corresponding to the given virtual address,
 * extracts the Page Frame Number (PFN) if present, and constructs the physical address.
 *
 * @param vaddr The virtual address to be converted.
 * @return The corresponding physical address, or 0 on failure.
 */
uintptr_t virtual_to_physical_address(uintptr_t vaddr)
{
    FILE* pagemap = nullptr;
    uintptr_t paddr = 0;
    auto page_size = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    uint64_t offset = (vaddr / page_size) * sizeof(uint64_t);
    uint64_t e = 0;

    pagemap = fopen("/proc/self/pagemap", "rbe");
    if (pagemap != nullptr) {
        if (fseek(pagemap, static_cast<int64_t>(offset), SEEK_SET) == 0) {
            if (fread(&e, sizeof(uint64_t), 1, pagemap) == 1) {
                // Check if the page is present
                if ((e & (1ULL << 63U)) != 0U) {
                    // Extract PFN
                    uint64_t pfn = e & ((1ULL << 54U) - 1);
                    paddr = pfn * page_size;
                    // Add offset within the page
                    paddr |= (vaddr & (page_size - 1));
                }
            }
        }
        fclose(pagemap);
    }

    return paddr;
}

/**
 * @brief Verifies that the physical address contents match the logical value of 'turn'.
 *
 * This function prints both the logical (virtual) and physical addresses of 'turn'. It then attempts
 * to read the value from /dev/mem at the physical address to confirm that it matches the current value
 * of 'turn'. Note that this step requires privileges to open /dev/mem.
 *
 * @param turn Pointer to the shared 'turn' variable.
 * @param i The process identifier (0 for parent, 1 for child).
 */
void verify_physical_address(const volatile int* turn, int i)
{
    auto vaddr = reinterpret_cast<uintptr_t>(turn);
    uintptr_t paddr = virtual_to_physical_address(vaddr);

    std::cout << "Process " << i << ": Turn logical address: 0x" << std::hex << vaddr << std::dec << "\n";
    std::cout << "Process " << i << ": Turn physical address: 0x" << std::hex << paddr << std::dec << "\n";

    // Attempt to open /dev/mem and read the value at the physical address
    if (paddr == 0) {
        std::cout << "Process " << i << ": Failed to convert vaddr to paddr.\n";
    } else {
        int memfd = open("/dev/mem", O_RDONLY | O_CLOEXEC);
        if (memfd < 0) {
            perror("open /dev/mem failed");
        } else {
            if (lseek(memfd, static_cast<off_t>(paddr), SEEK_SET) == static_cast<off_t>(-1)) {
                perror("lseek failed");
                close(memfd);
            } else {
                int val_at_paddr = 0;
                ssize_t rd = read(memfd, &val_at_paddr, sizeof(val_at_paddr));
                if (rd < 0) {
                    perror("read failed");
                } else {
                    std::cout << "Process " << i << ": Value stored at turn (logical): 0x"
                              << std::hex << *turn << std::dec << "\n";
                    std::cout << "Process " << i << ": Value read from physical address: 0x"
                              << std::hex << val_at_paddr << std::dec << "\n";

                    if (val_at_paddr == *turn) {
                        std::cout << "Process " << i << ": Verification successful: The value matches.\n";
                    } else {
                        std::cout << "Process " << i << ": Verification failed: The value does not match.\n";
                    }
                }
                close(memfd);
            }
        }
    }
}

/**
 * @brief The main routine for each process in Peterson's Algorithm.
 *
 * This function implements two iterations of the following:
 *  - Set the flag to indicate that this process wants to enter the critical section.
 *  - Set turn to the other process.
 *  - Busy-wait until it is safe to enter the critical section.
 *  - Enter the critical section, verify the physical address mapping of 'turn'.
 *  - Sleep for a moment to simulate some work in the critical section.
 *  - Exit the critical section, reset the flag.
 *  - Sleep to yield time to the other process.
 *
 * @param i The process identifier (0 for parent, 1 for child).
 * @param flag Pointer to the array of two flags (flag[0], flag[1]).
 * @param turn Pointer to the shared 'turn' variable.
 */
void peterson_algorithm(int i, volatile int* flag, volatile int* turn)
{
    int j = 1 - i;
    for (int count = 0; count < 2; count++) {
        // Indicate interest in entering the critical section
        flag[i] = 1;
        *turn = j;

        // Wait while the other process is interested and 'turn' is set to the other process
        while ((flag[j] != 0) && *turn == j) {
            // Busy-wait
        }

        // Critical section begins
        std::cout << "Process " << i << " entered critical section (iteration " << count << ")\n";

        // Verify the physical address mapping of 'turn'
        verify_physical_address(turn, i);

        // Simulate work in the critical section
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Process " << i << " leaving critical section (iteration " << count << ")\n\n";

        // Indicate exit from the critical section
        flag[i] = 0;

        // Yield time to the other process
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/**
 * @brief Program entry point.
 *
 * Allocates shared memory for the Peterson Algorithm's flags and turn variable. Forks a child process,
 * assigning roles (0 for parent, 1 for child). Both processes execute peterson_algorithm().
 * The parent waits for the child to finish and then releases the shared memory.
 *
 * @return EXIT_SUCCESS on successful execution, or EXIT_FAILURE on errors.
 */
int main()
{
    // Allocate shared memory for flag[0], flag[1], and turn
    int* shared_mem = static_cast<int*>(mmap(nullptr, sizeof(int) * 3, PROT_READ | PROT_WRITE,
                                             MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    volatile int* flag = reinterpret_cast<volatile int*>(shared_mem);      // flag[0], flag[1]
    volatile int* turn = reinterpret_cast<volatile int*>(shared_mem + 2);  // turn at shared_mem[2]

    // Initialize
    flag[0] = 0;
    flag[1] = 0;
    *turn = 0;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        munmap(shared_mem, sizeof(int) * 3);
        return EXIT_FAILURE;
    }

    // i = 0 for parent, i = 1 for child
    int i = (pid == 0) ? 1 : 0;
    peterson_algorithm(i, flag, turn);

    if (pid != 0) {
        // Parent waits for child process to finish
        wait(nullptr);
        munmap(shared_mem, sizeof(int) * 3);
    }

    return 0;
}
