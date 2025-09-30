#if defined(_WIN64) || defined(_WIN32)

#include <windows.h>
#include <iostream>

void pin_to_cpu(int cpuIndex) {
    HANDLE hProcess = GetCurrentProcess();
    DWORD_PTR mask = (1ULL << cpuIndex);  // choose CPU
    if (!SetProcessAffinityMask(hProcess, mask)) {
        std::cerr << "SetProcessAffinityMask failed: " << GetLastError() << "\n";
    }
    else {
        std::cout << "[+] Pinned process to CPU " << cpuIndex << "\n";
    }

    // Also set current thread affinity
    if (!SetThreadAffinityMask(GetCurrentThread(), mask)) {
        std::cerr << "SetThreadAffinityMask failed: " << GetLastError() << "\n";
    }
    else {
        std::cout << "[+] Pinned thread to CPU " << cpuIndex << "\n";
    }
}

void set_realtime_priority() {
    if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
        std::cerr << "SetPriorityClass failed: " << GetLastError() << "\n";
    }
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
        std::cerr << "SetThreadPriority failed: " << GetLastError() << "\n";
    }
    std::cout << "[+] Real-time priority set\n";
}

int affinity(int IN icpu) {
    int cpu = 1; // default CPU index
    if (icpu > 1) cpu = icpu;

    pin_to_cpu(cpu);
    set_realtime_priority();

    std::cout << "[*] Running on CPU " << cpu << " with high isolation...\n";
    return 0;
}

#endif
#if defined(__linux__)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/resource.h>

// Pin process to one CPU
void pin_to_cpu(int cpu) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        perror("sched_setaffinity");
        exit(1);
    }
    printf("[+] Pinned to CPU %d\n", cpu);
}

// Set SCHED_FIFO with high priority
void set_realtime(void) {
    struct sched_param p = { .sched_priority = 80 };
    if (sched_setscheduler(0, SCHED_FIFO, &p) == -1) {
        perror("sched_setscheduler");
        exit(1);
    }
    printf("[+] Real-time scheduling enabled\n");
}

// Move all IRQs away from this CPU
void move_irqs_off_cpu(int cpu) {
    DIR* d = opendir("/proc/irq");
    if (!d) {
        perror("opendir /proc/irq");
        return;
    }

    struct dirent* de;
    char path[256];
    char buf[128];
    while ((de = readdir(d))) {
        if (de->d_type != DT_DIR) continue;
        if (atoi(de->d_name) <= 0) continue; // skip non-IRQ dirs

        snprintf(path, sizeof(path), "/proc/irq/%s/smp_affinity", de->d_name);
        int fd = open(path, O_WRONLY);
        if (fd < 0) continue;

        // Build mask: all CPUs except "cpu"
        // Here assume <= 64 CPUs (1 mask word)
        unsigned long long mask = ~((1ULL << cpu));
        snprintf(buf, sizeof(buf), "%llx", mask);

        if (write(fd, buf, strlen(buf)) < 0) {
            // Not fatal, some IRQs can't be changed
        }
        else {
            printf("[+] IRQ %s moved off CPU %d\n", de->d_name, cpu);
        }
        close(fd);
    }
    closedir(d);
}

// Optional: disable sibling hyperthread
void disable_smt_sibling(int cpu) {
    char path[256];
    snprintf(path, sizeof(path),
        "/sys/devices/system/cpu/cpu%d/online", cpu + 1);
    // naive: assumes sibling = cpu+1, you should parse topology for real mapping

    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        if (write(fd, "0", 1) > 0) {
            printf("[+] Disabled sibling CPU %d\n", cpu + 1);
        }
        close(fd);
    }
    else {
        printf("[!] Could not disable sibling, adjust topology parsing\n");
    }
}

#define IN

int affinity(int IN icpu) {
    if (geteuid() != 0) {
        fprintf(stderr, "Must run as root\n");
        return 1;
    }

    int cpu = 4; // choose target CPU here
    if (icpu > 1) cpu = icpu;

    pin_to_cpu(cpu);
    set_realtime();
    move_irqs_off_cpu(cpu);
    disable_smt_sibling(cpu);

    printf("[*] Core %d isolated. Running workload...\n", cpu);

    return 0;
}

#endif