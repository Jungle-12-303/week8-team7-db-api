# 미니 DBMS API 서버 MVP 계획

## Summary
- `week8-team7-db-api`의 기존 SQL 처리기와 B+Tree 인덱스는 내부 DB 엔진으로 그대로 유지한다.
- 새로 추가할 것은 C 기반 HTTP API 서버, thread pool, job queue, 런타임 진입점, 기능/동시성 테스트다.
- Windows/macOS 지원은 네이티브 빌드가 아니라 Docker 기반 Linux 실행 환경으로 통일한다.
- MVP 동시성 기본값은 `thread pool + 전역 engine mutex`로 잡는다. worker는 병렬로 대기/처리하지만, 기존 CSV/B+Tree 엔진 실행 구간은 안전하게 직렬화한다.

## Key Changes
- 엔진 wrapper 추가
  - `engine_execute_sql(sql, schema_dir, data_dir)` 형태의 공통 함수를 만든다.
  - 내부에서 `lex_sql -> parse_statement -> execute_statement`를 호출한다.
  - `execute_statement()`가 쓰는 `FILE *out` 출력은 `tmpfile()`로 받아 문자열 응답으로 변환한다.
  - 반환은 성공 여부, 실행 메시지, 출력 문자열을 포함하는 구조체로 한다.
- API 서버 추가
  - 서버 바이너리 예: `db_server <port> [worker_count]`
  - listener thread가 연결을 accept하고 HTTP 요청을 읽는다.
  - job queue에 client socket과 SQL 요청 작업을 넣는다.
  - worker threads가 queue에서 작업을 꺼내 엔진 wrapper를 호출한다.
  - 엔진 호출 전후로 전역 mutex를 걸어 CSV append, B+Tree registry, next_id race를 방지한다.
- HTTP 계층 추가
  - `GET /health`와 `POST /query`를 최소 엔드포인트로 둔다.
  - `POST /query`는 SQL 문자열을 입력으로 받아 엔진 wrapper 호출 결과를 JSON으로 반환한다.
  - 잘못된 method, path, body는 안전하게 4xx/5xx 응답으로 변환한다.
- 런타임 분리
  - 실제 서버 프로세스 진입점, 설정 로딩, 포트와 worker 연결, schema/data 경로 연결은 별도 런타임 계층에서 담당한다.
- Docker 실행 환경
  - Dockerfile은 Ubuntu 기반 개발 환경을 유지하되 서버/클라이언트 빌드에 필요한 POSIX socket/pthread 기준으로 작성한다.
  - Makefile에 `server`, `client`, `api-test` 타깃을 추가한다.
  - Windows/macOS 사용자는 동일하게 Docker Compose로 컨테이너에 들어가 `make test`, `make server`, `make client`를 실행한다.
  - 네이티브 Windows Winsock/MSVC 지원은 MVP 범위 밖으로 둔다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. `work.md`, `error.md`, `context.md`, `.codex/`

- 공통 요구사항, 공통 아키텍처, 공통 API 명세의 원본은 루트 `PLAN.md`다.
- 하위 폴더는 루트 `PLAN.md`를 복사하지 않고 참조만 한다.
- 하위 문서가 루트 `PLAN.md`와 충돌하면 루트 `PLAN.md`를 우선하고, 변경 필요 사항은 루트 `context.md` 또는 `error.md`에 기록한다.

## 트랙 책임 경계
- 루트
  - 공통 명세 관리
  - 공용 파일 소유
  - 통합 조정과 마무리
- `week8-team7-db-api`
  - SQL 처리기
  - 실행기
  - B+Tree 인덱스
  - 기존 엔진 테스트
- `SERVER-CORE`
  - 엔진 wrapper
  - 출력 캡처
  - 엔진 실행 경계와 mutex 연동 지점
- `SERVER-HTTP`
  - HTTP request parsing
  - JSON 응답 생성
  - `/health`, `/query` 처리
- `SERVER-CONCURRENCY`
  - worker pool
  - job queue
  - lock 정책
  - graceful shutdown drain
- `SERVER-RUNTIME`
  - `server_main`
  - 실행 설정
  - 포트/worker 연결
  - schema/data 경로 연결
- `TEST-ENGINE-ADAPTER`
  - 엔진 어댑터 단위 검증
- `TEST-HTTP-FUNCTIONAL`
  - HTTP 기능 검증
- `TEST-CONCURRENCY`
  - 병렬 SQL 및 락 정책 검증
- `TEST-EDGE-FAILURE`
  - 엣지 케이스와 실패 복원 검증

- 현재 작업 단위는 루트 바로 아래의 `SERVER-*`, `TEST-*` 폴더로 분리한다.
- 별도 트랙을 추가하더라도 공통 계약 변경은 루트 `PLAN.md`에서 먼저 조정한다.

## 공용 파일 소유 범위
- 루트 소유 파일
  - `PLAN.md`
  - `agent.md`
  - `context.md`
  - `work.md`
  - `error.md`
  - `commit-rules.md`
  - `Dockerfile`
  - `docker-compose.yml`
  - `.dockerignore`
  - `Makefile`
- 루트는 컨트롤 타워 역할을 하며, 공용 파일 변경과 최종 연결 작업을 담당한다.
- 하위 폴더는 루트 소유 파일을 임의로 수정하지 않고, 필요 시 루트 문서에 변경 요청 또는 업데이트 메모를 남긴다.

## Test Plan
- 기존 테스트 유지
  - `make test`
  - 기존 375개 테스트가 계속 통과해야 한다.
- 엔진 wrapper 단위 테스트
  - 정상 `SELECT * FROM student;`
  - 정상 `INSERT INTO ...`
  - `WHERE id = <number>` 인덱스 조회
  - 문법 오류 SQL
  - 존재하지 않는 테이블/컬럼
- 서버 기능 테스트
  - `GET /health` 정상 응답 확인
  - `POST /query`로 `SELECT`, `INSERT`, `SELECT ... WHERE id = ...` 요청 확인
  - 실패 SQL 요청 시 일관된 오류 JSON 응답 확인
  - 잘못된 method/path/body에 대해 안전한 4xx/5xx 응답 확인
- 동시성 테스트
  - 여러 클라이언트 프로세스가 동시에 `SELECT` 요청을 보내는 케이스
  - 여러 클라이언트가 동시에 `INSERT` 요청을 보내는 케이스
  - 전역 engine mutex 때문에 결과는 직렬 처리되지만, 서버가 crash 없이 모든 응답을 반환해야 한다.
  - `INSERT` 후 `SELECT id` 또는 `WHERE id`로 누락/중복이 없는지 확인한다.

## Assumptions And Defaults
- MVP 기본 동시성 모델은 `thread pool + global engine mutex`다.
- read/write lock, table-level lock은 후속 개선 선택지로 문서화만 한다.
- 추가 차별점 기능은 이번 MVP 구현 후 보류한다.
- Windows/macOS 호환은 Docker 기반 실행으로 충족한다.
- API는 HTTP 기반으로 시작하고, 응답은 JSON 구조를 사용한다.
- 별도 GUI/브라우저 클라이언트는 범위 밖이며, smoke test는 스크립트와 테스트 폴더에서 수행한다.
- `Dockerfile`, `docker-compose.yml`, `Makefile`은 별도 `DOCKER/` 폴더로 분리하지 않고 루트에서 관리한다.
