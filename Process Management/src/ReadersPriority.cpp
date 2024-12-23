/**
 * @file main.cpp
 * @brief A demonstration of the Readers-Writers problem using the Readers
 * Priority solution.
 *
 * This example code implements the Readers Priority strategy for the classic
 * Readers-Writers problem. Multiple reader threads and writer threads are
 * created. Readers have priority if they start reading before a writer acquires
 * the write lock, ensuring no writer can proceed if at least one reader is
 * already reading.
 *
 * Usage:
 *   Compile and run. The program runs infinitely, simulating both readers and
 * writers.
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

/**
 * @brief ANSI color reset code.
 */
#define RESET "\033[0m"

/**
 * @brief ANSI color code for green text.
 */
#define GREEN "\033[32m"

/**
 * @brief ANSI color code for red text.
 */
#define RED "\033[31m"

/**
 * @brief Retrieves the current local time in a string format with milliseconds.
 *
 * This function uses `std::chrono` to get the current system time, and formats
 * it into a string in the format `YYYY-MM-DD HH:MM:SS.mmm`.
 *
 * @return A string representing the current local time with millisecond
 * precision.
 */
std::string current_time() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  auto duration = now.time_since_epoch();
  auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;

  std::tm buf{};
  localtime_r(&in_time_t, &buf);

  std::stringstream ss;
  ss << std::put_time(&buf, "%Y-%m-%d %X") << '.' << std::setw(3)
     << std::setfill('0') << milliseconds.count();
  return ss.str();
}

/**
 * @class ReadersPriority
 * @brief Implements the Readers Priority solution to the Readers-Writers
 * problem.
 *
 * In this Readers Priority approach:
 * - Multiple readers can read concurrently if no writer is writing.
 * - A writer can write only if no reader is reading and no other writer is
 * writing.
 * - Readers have priority: once a reader starts reading, it prevents writers
 * from acquiring the lock until all readers have finished.
 */
class ReadersPriority {
private:
  /**
   * @brief Mutex to protect the shared counter `read_count`.
   */
  std::mutex mtx;

  /**
   * @brief Mutex used by writers. If locked, writers are writing (or waiting to
   * write).
   */
  std::mutex wrt;

  /**
   * @brief Number of active readers.
   */
  int read_count = 0;

  /**
   * @brief Mutex to protect output operations to `std::cout`.
   */
  std::mutex cout_mtx;

public:
  /**
   * @brief Default constructor.
   */
  ReadersPriority() = default;

  /**
   * @brief Function executed by reader threads.
   *
   * Each reader enters a loop:
   *  - Acquires a lock on `mtx` to safely increment `read_count`.
   *  - If this thread is the first reader (`read_count == 1`), it locks `wrt`
   * to block writers.
   *  - Simulates a read operation and logs it to `std::cout`.
   *  - Decrements `read_count`, and if it is the last reader (`read_count ==
   * 0`), unlocks `wrt`.
   *  - Sleeps briefly before attempting to read again.
   *
   * @param reader_id An integer identifier for the reader (for logging).
   */
  void reader(int reader_id) {
    while (true) {
      // Entry section: lock mtx to increment read_count
      {
        std::unique_lock<std::mutex> lock(mtx);
        read_count++;
        if (read_count == 1) {
          wrt.lock(); // First reader locks wrt to block writers
        }
      }

      // Critical section: reading
      {
        std::lock_guard<std::mutex> lock(cout_mtx);
        std::cout << "[" << current_time() << "] " << GREEN << "Reader "
                  << reader_id << " is reading." << RESET << "\n";
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(100)); // Simulate read time

      // Exit section: decrement read_count, unlock wrt if this is the last
      // reader
      {
        std::unique_lock<std::mutex> lock(mtx);
        read_count--;
        if (read_count == 0) {
          wrt.unlock(); // Last reader unlocks wrt to allow writers
        }
      }

      // Pause before next read
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  /**
   * @brief Function executed by writer threads.
   *
   * Each writer enters a loop:
   *  - Locks `wrt` to gain exclusive writing access.
   *  - Simulates a write operation and logs it to `std::cout`.
   *  - Unlocks `wrt` to allow other readers or writers.
   *  - Sleeps briefly before attempting to write again.
   *
   * @param writer_id An integer identifier for the writer (for logging).
   */
  void writer(int writer_id) {
    while (true) {
      // Entry section: lock wrt
      wrt.lock();

      // Critical section: writing
      {
        std::lock_guard<std::mutex> lock(cout_mtx);
        std::cout << "[" << current_time() << "] " << RED << "Writer "
                  << writer_id << " is writing." << RESET << "\n";
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(150)); // Simulate write time

      // Exit section: unlock wrt
      wrt.unlock();

      // Pause before next write
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  }
};

/**
 * @brief Main function where reader and writer threads are created.
 *
 * Creates multiple reader threads and writer threads. Each thread runs
 * indefinitely, demonstrating the Readers Priority approach for the
 * Readers-Writers problem.
 *
 * @return Exit code (0 for normal termination).
 */
int main() {
  ReadersPriority rw;
  std::vector<std::thread> threads;

  // Create multiple reader threads
  for (int i = 1; i <= 5; ++i) {
    threads.emplace_back(&ReadersPriority::reader, &rw, i);
  }

  // Create multiple writer threads
  for (int i = 1; i <= 2; ++i) {
    threads.emplace_back(&ReadersPriority::writer, &rw, i);
  }

  // Wait for all threads to finish (in this example, they run indefinitely)
  for (auto &th : threads) {
    th.join();
  }

  return 0;
}