/**
 * @file main_writers_priority.cpp
 * @brief A demonstration of the Readers-Writers problem using the Writers
 * Priority solution.
 *
 * This example code implements the Writers Priority strategy for the classic
 * Readers-Writers problem. Multiple reader threads and writer threads are
 * created. Writers have priority, meaning a writer waiting to write should
 * block new readers from proceeding.
 *
 * Usage:
 *   Compile and run. The program runs infinitely, simulating both readers and
 * writers.
 */

#include <chrono>
#include <condition_variable>
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
 * @brief ANSI color code for green text (used by readers).
 */
#define GREEN "\033[32m"

/**
 * @brief ANSI color code for red text (used by writers).
 */
#define RED "\033[31m"

/**
 * @brief Retrieves the current local time in a string format with milliseconds
 * (thread-safe).
 *
 * This function uses `std::chrono` to get the current system time and formats
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
 * @class WritersPriority
 * @brief Implements a Writers Priority solution to the Readers-Writers problem.
 *
 * In this Writers Priority approach:
 * - Writers have priority to acquire the lock over new readers.
 * - A writer will block incoming readers when it is waiting for or holding the
 * write lock.
 */
class WritersPriority {
private:
  /**
   * @brief Mutex to protect shared state (e.g., counters, flags).
   */
  std::mutex mtx;

  /**
   * @brief Mutex used by writers. If locked, a writer is writing (or waiting to
   * write).
   */
  std::mutex wrt;

  /**
   * @brief Condition variable to signal state changes to waiting threads.
   */
  std::condition_variable cond;

  /**
   * @brief Current number of active readers.
   */
  int read_count = 0;

  /**
   * @brief Current number of writers either waiting or writing.
   */
  int write_count = 0;

  /**
   * @brief Flag to indicate if the system currently prefers writers over
   * readers.
   */
  bool prefer_writers = true;

  /**
   * @brief Indicates whether the `wrt` mutex is currently locked by a writer.
   */
  bool wrt_locked = false;

  /**
   * @brief Mutex to protect output operations (e.g. writing to std::cout).
   */
  std::mutex cout_mtx;

public:
  /**
   * @brief Default constructor.
   */
  WritersPriority() = default;

  /**
   * @brief Function executed by reader threads.
   *
   * Each reader enters a loop:
   *  - Waits while any writer is waiting.
   *  - Increments the reader count, locking `wrt` if this is the first reader.
   *  - Performs reading operations (simulated with a sleep).
   *  - Decrements reader count, unlocking `wrt` if this is the last reader.
   *  - Notifies all threads waiting on the condition variable.
   *  - Sleeps briefly before attempting to read again.
   *
   * @param reader_id An integer identifier for the reader (for logging).
   */
  void reader(int reader_id) {
    while (true) {
      {
        std::unique_lock<std::mutex> lock(mtx);
        // Wait until no writers are waiting
        cond.wait(lock, [this]() { return write_count == 0; });
        read_count++;
        if (read_count == 1) {
          wrt.lock(); // First reader locks wrt to block writers
        }
      }

      // Critical section: reading
      {
        std::lock_guard<std::mutex> cout_lock(cout_mtx);
        std::cout << "[" << current_time() << "] " << GREEN << "Reader "
                  << reader_id << " is reading." << RESET << "\n";
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(100)); // Simulate read time

      {
        std::unique_lock<std::mutex> lock(mtx);
        read_count--;
        if (read_count == 0) {
          wrt.unlock();      // Last reader unlocks wrt to allow writers
          cond.notify_all(); // Wake up waiting writers/readers
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
   *  - Increments the writer count.
   *  - Waits until no readers are reading (`read_count == 0`) and `wrt` is not
   * locked.
   *  - Locks `wrt` and sets `wrt_locked` to true.
   *  - Performs writing operations (simulated with a sleep).
   *  - Decrements writer count, unlocks `wrt`, and resets `wrt_locked`.
   *  - Notifies all threads waiting on the condition variable.
   *  - Sleeps briefly before attempting to write again.
   *
   * @param writer_id An integer identifier for the writer (for logging).
   */
  void writer(int writer_id) {
    while (true) {
      {
        std::unique_lock<std::mutex> lock(mtx);
        write_count++;
        // Wait until no reader is active and wrt is not locked
        cond.wait(lock,
                  [this]() { return (read_count == 0) && (!wrt_locked); });
        wrt_locked = true;
        wrt.lock(); // Lock wrt to block others
      }

      // Critical section: writing
      {
        std::lock_guard<std::mutex> cout_lock(cout_mtx);
        std::cout << "[" << current_time() << "] " << RED << "Writer "
                  << writer_id << " is writing." << RESET << "\n";
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(150)); // Simulate write time

      {
        std::unique_lock<std::mutex> lock(mtx);
        wrt.unlock();
        write_count--;
        wrt_locked = false;
        prefer_writers = true; // Reinforce writers preference
        cond.notify_all();     // Wake up waiting threads
      }

      // Pause before next write
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  }
};

/**
 * @brief Main function where reader and writer threads are created.
 *
 * Creates multiple reader threads and writer threads. Each thread runs
 * indefinitely, demonstrating the Writers Priority approach for the
 * Readers-Writers problem.
 *
 * @return Exit code (0 for normal termination).
 */
int main() {
  WritersPriority rw;
  std::vector<std::thread> threads;

  // Create multiple reader threads
  for (int i = 1; i <= 5; ++i) {
    threads.emplace_back(&WritersPriority::reader, &rw, i);
  }

  // Create multiple writer threads
  for (int i = 1; i <= 2; ++i) {
    threads.emplace_back(&WritersPriority::writer, &rw, i);
  }

  // Wait for all threads to finish (in this example, they run indefinitely)
  for (auto &th : threads) {
    th.join();
  }

  return 0;
}