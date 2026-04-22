/*
 * lock_manager.h
 *
 * SQL 작업의 read/write 성격에 맞춰 실행 구간 직렬화를 담당하는 공개 헤더다.
 */
#ifndef SERVER_CONCURRENCY_LOCK_MANAGER_H
#define SERVER_CONCURRENCY_LOCK_MANAGER_H

#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* worker가 작업 실행 전에 요청할 수 있는 락 모드다. */
typedef enum {
    CONCURRENCY_LOCK_MODE_NONE = 0,
    CONCURRENCY_LOCK_MODE_READ,
    CONCURRENCY_LOCK_MODE_WRITE
} ConcurrencyLockMode;

/*
 * 루트 계획의 보수적 기본값을 유지할 수 있도록
 * 전체 직렬화와 read-read 병렬 정책을 모두 지원한다.
 */
typedef enum {
    CONCURRENCY_LOCK_POLICY_READERS_PARALLEL = 0,
    CONCURRENCY_LOCK_POLICY_SERIAL_ALL
} ConcurrencyLockPolicy;

typedef struct {
    ConcurrencyLockPolicy policy;
    size_t active_readers;
    size_t waiting_writers;
    int writer_active;
    int ready;
    pthread_mutex_t mutex;
    pthread_cond_t readers_cv;
    pthread_cond_t writers_cv;
} ConcurrencyLockManager;

/* 락 매니저를 초기화한다. 성공 시 1, 실패 시 0을 반환한다. */
int concurrency_lock_manager_init(ConcurrencyLockManager *manager, ConcurrencyLockPolicy policy);

/* 락 매니저가 보유한 동기화 자원을 해제한다. */
void concurrency_lock_manager_destroy(ConcurrencyLockManager *manager);

/* 지정한 모드로 진입 권한을 획득한다. */
void concurrency_lock_manager_acquire(ConcurrencyLockManager *manager, ConcurrencyLockMode mode);

/* acquire와 짝을 이루는 락 해제 함수다. */
void concurrency_lock_manager_release(ConcurrencyLockManager *manager, ConcurrencyLockMode mode);

/*
 * SQL 선두 키워드만 보고 보수적으로 락 모드를 분류한다.
 * SELECT만 READ로 보고, 나머지는 WRITE로 처리한다.
 */
ConcurrencyLockMode concurrency_lock_mode_from_sql(const char *sql);

/* 디버깅과 로그를 위해 모드 이름을 돌려준다. */
const char *concurrency_lock_mode_name(ConcurrencyLockMode mode);

/* 디버깅과 로그를 위해 정책 이름을 돌려준다. */
const char *concurrency_lock_policy_name(ConcurrencyLockPolicy policy);

#ifdef __cplusplus
}
#endif

#endif
