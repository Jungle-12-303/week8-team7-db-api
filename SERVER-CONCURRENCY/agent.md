# SERVER-CONCURRENCY Agent Brief

## 목적
- thread pool, job queue, lock manager를 통해 서버 동시성 정책을 담당한다.
- req_id 기반 worker/lock trace 연결 지점을 제공한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 MVP 기본값 `serial_all`과 후속 최적화 정책 `read-read 병렬 / write-* 직렬` 구분을 따른다.
- 루트 `agent.md`의 동시성/안정성 검증 규칙을 따른다.
- live 검증은 Docker runtime과 `TEST-CONCURRENCY` 케이스를 기준으로 확인한다.

## 담당 범위
- worker thread 생성과 종료
- bounded job queue
- lock manager
- graceful shutdown 시 drain 정책
- worker 할당 시점과 worker index 노출을 위한 런타임 연동 지점 제공
- `SERVER-HTTP` 요청 컨텍스트와 runtime job context를 잇는 trace 구조 경계 제공
- `[LOCK]` 고정 형식 로그와 상태 전이 로그 출력 책임
- queue rejection 경로의 resource safety 보장

## 범위 제외
- HTTP request parsing
- 엔진 결과 직렬화
- 서버 프로세스 진입점

## 인터페이스
- 입력: 실행 작업 단위
- 출력: 실행 작업 완료/실패 통지
- 작업은 optional `assigned`, `notify`, `cleanup` 콜백을 가질 수 있다.
- `concurrency_thread_pool_current_worker_index()`로 현재 worker index를 읽을 수 있다.
- `SERVER-HTTP`는 per-request `http_request_trace`를 통해 req_id, method, path, status, worker를 runtime과 공유할 수 있다.
- `SELECT`와 `INSERT` 성격에 따라 lock 정책을 적용하되, MVP 기본값은 `serial_all`을 우선한다.
- lock manager는 `[LOCK] | event=... | worker=... | requested=... | effective=... | active_readers=... | waiting_writers=... | writer_active=... |` 형식 로그를 직접 출력한다.
- runtime 경계에서는 `assigned`/`notify` 콜백으로 `스레드 할당`, `작업 종료` 이벤트를 req_id 기준으로 연결한다.

## 소유 경로
- `SERVER-CONCURRENCY/include/**`
- `SERVER-CONCURRENCY/src/**`

## 의존성
- `SERVER-CORE`
- `SERVER-HTTP`
- `SERVER-RUNTIME`

## 완료 조건
- thread pool과 queue 책임이 문서화된다.
- `serial_all` 기본값과 `read-read 병렬 / write-* 직렬` 확장 정책이 함께 명시된다.
- 상위 계층이 worker 할당 이벤트와 worker index를 연결할 수 있는 인터페이스가 제공된다.
- lock state 전이가 live 로그에서 확인 가능한 고정 포맷으로 남는다.
- queue rejection 경로에서 자원 정리 위험이 남아 있지 않다.

## 테스트 기준
- 병렬 `SELECT` 시 동시 실행 근거가 있어야 한다.
- `INSERT`와 혼합 요청에서 일관성 보장 정책이 설명 가능해야 한다.
- worker index accessor와 `assigned` 콜백이 스모크 테스트 수준에서 검증 가능해야 한다.
- `serial_all` 정책에서 `requested=read`, `effective=write` 승격과 `락 대기/획득/해제` 로그가 Docker live runtime에서 확인 가능해야 한다.
- Docker live runtime 기준 `TEST-CONCURRENCY` 주요 4케이스가 유지되어야 한다.
