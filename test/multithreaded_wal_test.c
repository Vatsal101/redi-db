#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io.h"
#include "kv.h"
#include "wal.h"

#define TEST_DB "multithreaded_wal_test.db"
#define TEST_WAL "multithreaded_wal_test.db.wal"
#define THREAD_COUNT 8
#define OPS_PER_THREAD 50

typedef struct {
  int thread_id;
  int operations;
  int failed;
} worker_arg_t;

static void timeout_handler(int signum) {
  (void)signum;
  const char msg[] =
      "\n[FAIL] test timed out; possible WAL group-commit deadlock\n";
  write(STDERR_FILENO, msg, sizeof(msg) - 1);
  _exit(2);
}

static void install_timeout(void) {
  signal(SIGALRM, timeout_handler);
  alarm(20);
}

static void cleanup_test_files(void) {
  unlink(TEST_DB);
  unlink(TEST_WAL);
}

static void print_test_result(const char *test_name, int passed) {
  printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

static void *db_put_worker(void *arg) {
  worker_arg_t *worker = (worker_arg_t *)arg;

  for (int i = 0; i < worker->operations; i++) {
    char key[64];
    char value[64];

    snprintf(key, sizeof(key), "db_put_t%d_k%d", worker->thread_id, i);
    snprintf(value, sizeof(value), "db_value_t%d_k%d", worker->thread_id, i);

    if (db_put_table(key, value) != 0) {
      worker->failed = 1;
      return NULL;
    }
  }

  return NULL;
}

static void *db_mixed_worker(void *arg) {
  worker_arg_t *worker = (worker_arg_t *)arg;

  for (int i = 0; i < worker->operations; i++) {
    char key[64];
    char value[64];

    snprintf(key, sizeof(key), "db_mixed_t%d_k%d", worker->thread_id, i);
    snprintf(value, sizeof(value), "db_mixed_value_t%d_k%d", worker->thread_id,
             i);

    if (db_put_table(key, value) != 0) {
      worker->failed = 1;
      return NULL;
    }

    if (i % 2 == 1 && db_delete_table(key) != 0) {
      worker->failed = 1;
      return NULL;
    }
  }

  return NULL;
}

static void *direct_wal_put_worker(void *arg) {
  worker_arg_t *worker = (worker_arg_t *)arg;
  WalManager *wal = get_wal_manager();

  if (!wal) {
    worker->failed = 1;
    return NULL;
  }

  for (int i = 0; i < worker->operations; i++) {
    char key[64];
    char value[64];

    snprintf(key, sizeof(key), "wal_only_t%d_k%d", worker->thread_id, i);
    snprintf(value, sizeof(value), "wal_only_value_t%d_k%d", worker->thread_id,
             i);

    if (wal_commit_put(wal, key, value) != 0) {
      worker->failed = 1;
      return NULL;
    }
  }

  return NULL;
}

static int run_threads(void *(*worker_fn)(void *), worker_arg_t *args,
                       int operations) {
  pthread_t threads[THREAD_COUNT];

  for (int i = 0; i < THREAD_COUNT; i++) {
    args[i].thread_id = i;
    args[i].operations = operations;
    args[i].failed = 0;

    if (pthread_create(&threads[i], NULL, worker_fn, &args[i]) != 0)
      return 0;
  }

  for (int i = 0; i < THREAD_COUNT; i++) {
    if (pthread_join(threads[i], NULL) != 0)
      return 0;
  }

  for (int i = 0; i < THREAD_COUNT; i++) {
    if (args[i].failed)
      return 0;
  }

  return 1;
}

static int verify_key_value(const char *key, const char *expected_value) {
  char *actual = db_get_table(key);
  int ok = actual != NULL && strcmp(actual, expected_value) == 0;
  free(actual);
  return ok;
}

static int verify_absent(const char *key) {
  char *actual = db_get_table(key);
  int ok = actual == NULL;
  free(actual);
  return ok;
}

static int test_concurrent_db_puts_reopen(void) {
  printf("\n=== Concurrent DB PUTs + reopen ===\n");
  cleanup_test_files();

  if (db_create(TEST_DB) != 0)
    return 0;

  worker_arg_t args[THREAD_COUNT];
  if (!run_threads(db_put_worker, args, OPS_PER_THREAD)) {
    db_close();
    return 0;
  }

  db_close();

  if (db_open(TEST_DB) != 0)
    return 0;

  for (int t = 0; t < THREAD_COUNT; t++) {
    for (int i = 0; i < OPS_PER_THREAD; i++) {
      char key[64];
      char value[64];

      snprintf(key, sizeof(key), "db_put_t%d_k%d", t, i);
      snprintf(value, sizeof(value), "db_value_t%d_k%d", t, i);

      if (!verify_key_value(key, value)) {
        db_close();
        return 0;
      }
    }
  }

  db_close();
  return 1;
}

static int test_concurrent_db_mixed_put_delete_reopen(void) {
  printf("\n=== Concurrent DB PUT/DELETE + reopen ===\n");
  cleanup_test_files();

  if (db_create(TEST_DB) != 0)
    return 0;

  worker_arg_t args[THREAD_COUNT];
  if (!run_threads(db_mixed_worker, args, OPS_PER_THREAD)) {
    db_close();
    return 0;
  }

  db_close();

  if (db_open(TEST_DB) != 0)
    return 0;

  for (int t = 0; t < THREAD_COUNT; t++) {
    for (int i = 0; i < OPS_PER_THREAD; i++) {
      char key[64];
      char value[64];

      snprintf(key, sizeof(key), "db_mixed_t%d_k%d", t, i);
      snprintf(value, sizeof(value), "db_mixed_value_t%d_k%d", t, i);

      if (i % 2 == 1) {
        if (!verify_absent(key)) {
          db_close();
          return 0;
        }
      } else if (!verify_key_value(key, value)) {
        db_close();
        return 0;
      }
    }
  }

  db_close();
  return 1;
}

static int test_concurrent_direct_wal_recovery(void) {
  printf("\n=== Concurrent direct WAL commits + recovery ===\n");
  cleanup_test_files();

  if (db_create(TEST_DB) != 0)
    return 0;

  worker_arg_t args[THREAD_COUNT];
  if (!run_threads(direct_wal_put_worker, args, OPS_PER_THREAD)) {
    db_close();
    return 0;
  }

  db_close();

  if (db_open(TEST_DB) != 0)
    return 0;

  for (int t = 0; t < THREAD_COUNT; t++) {
    for (int i = 0; i < OPS_PER_THREAD; i++) {
      char key[64];
      char value[64];

      snprintf(key, sizeof(key), "wal_only_t%d_k%d", t, i);
      snprintf(value, sizeof(value), "wal_only_value_t%d_k%d", t, i);

      if (!verify_key_value(key, value)) {
        db_close();
        return 0;
      }
    }
  }

  db_close();
  return 1;
}

static int test_group_commit_reduces_fsyncs(void) {
  printf("\n=== Group commit reduces fsyncs ===\n");
  cleanup_test_files();

  if (db_create(TEST_DB) != 0)
    return 0;

  WalManager *wal = get_wal_manager();
  if (!wal) {
    db_close();
    return 0;
  }

  wal_reset_fsync_count(wal);

  worker_arg_t args[THREAD_COUNT];
  if (!run_threads(direct_wal_put_worker, args, OPS_PER_THREAD)) {
    db_close();
    return 0;
  }

  uint64_t fsync_count = wal_get_fsync_count(wal);
  uint64_t commit_count = THREAD_COUNT * OPS_PER_THREAD;

  printf("  commits: %llu\n", (unsigned long long)commit_count);
  printf("  wal fsyncs: %llu\n", (unsigned long long)fsync_count);

  db_close();

  return fsync_count > 0 && fsync_count < commit_count;
}

int main(void) {
  install_timeout();

  printf("Multithreaded WAL Test Suite\n");
  printf("============================\n");

  struct {
    const char *name;
    int (*test_func)(void);
  } tests[] = {
      {"Concurrent DB PUTs + reopen", test_concurrent_db_puts_reopen},
      {"Concurrent DB PUT/DELETE + reopen",
       test_concurrent_db_mixed_put_delete_reopen},
      {"Concurrent direct WAL commits + recovery",
       test_concurrent_direct_wal_recovery},
      {"Group commit reduces fsyncs", test_group_commit_reduces_fsyncs},
  };

  int total = sizeof(tests) / sizeof(tests[0]);
  int passed = 0;

  for (int i = 0; i < total; i++) {
    int result = tests[i].test_func();
    if (result)
      passed++;
    print_test_result(tests[i].name, result);
  }

  cleanup_test_files();
  alarm(0);

  printf("\nTotal tests: %d\n", total);
  printf("Passed: %d\n", passed);
  printf("Failed: %d\n", total - passed);

  return passed == total ? 0 : 1;
}
