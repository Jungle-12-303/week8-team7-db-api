# 미니 DBMS API 서버 루트 계획

## Summary
- 기존 SQL 엔진은 `week8-team7-db-api`를 그대로 사용한다.
- 루트 런타임은 `SERVER-CORE`, `SERVER-HTTP`, `SERVER-CONCURRENCY`, `SERVER-RUNTIME`를 조립해 `build/bin/db_server`를 만든다.
- 현재 공개 HTTP API는 최소 범위인 `GET /health`, `POST /query` 두 개다.
- `POST /query`는 raw SQL text body를 받아 엔진으로 넘기고 JSON으로 결과를 반환한다.
- 동시성은 API 서버 계층에서 구현되어 있다. 요청 수용, 큐잉, worker 배정은 병렬이지만 실제 DB 실행 구간은 현재 `CONCURRENCY_LOCK_POLICY_SERIAL_ALL` 정책으로 직렬화한다.
- 표준 실행 환경은 루트 `Dockerfile`과 `compose.yaml` 기반 Linux 컨테이너다.

## 현재 구조

### `week8-team7-db-api`
- lexer, parser, executor, CSV storage, B+Tree index를 포함한 기존 SQL 엔진
- 현재 서버는 이 디렉터리의 실행 로직을 재사용한다

### `SERVER-CORE`
- 서버가 호출하는 공통 엔진 실행 경계 제공
- `server_core_execute(...)`
- `server_core_week8_engine_run(...)`

### `SERVER-HTTP`
- HTTP request parsing
- `/health`, `/query` routing
- JSON 응답 생성
- raw SQL request body 검증

### `SERVER-CONCURRENCY`
- bounded job queue
- worker thread pool
- lock manager

### `SERVER-RUNTIME`
- listener socket 생성
- accept loop
- thread pool submit
- HTTP와 engine callback wiring
- 실행 설정 로딩

### `TEST-*`
- `TEST-ENGINE-ADAPTER`: `SERVER-CORE` 공개 API 계약 검증
- `TEST-HTTP-FUNCTIONAL`: HTTP 계약 검증
- `TEST-CONCURRENCY`: 동시 요청 및 결과 무결성 검증
- `TEST-EDGE-FAILURE`: 경계 실패 케이스 검증

## API 계약

### `GET /health`
- 성공 시 `200`
- 응답 필드
  - `ok`
  - `path`
  - `status_code`
  - `message`

예시:

```json
{
  "ok": true,
  "path": "/health",
  "status_code": 200,
  "message": "server is healthy"
}
```

### `POST /query`
- 요청 body는 raw SQL text
- 권장 헤더는 `Content-Type: text/plain; charset=utf-8`
- 선택적 디버그 헤더로 `X-Debug-Sleep-Ms: <0~10000>`를 지원한다
- `X-Debug-Sleep-Ms`는 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 적용된다
- 성공 시 `200`
- SQL 실행 실패는 현재 `400`
- 잘못된 `X-Debug-Sleep-Ms` 값은 `400` + `error_code: "invalid_header"`로 응답한다

성공 응답 필드:
- `ok`
- `path`
- `status_code`
- `message`
- `affected_rows`
- `output_text`

실패 응답 필드:
- `ok`
- `path`
- `status_code`
- `error_code`
- `message`

예시:

```json
{
  "ok": true,
  "path": "/query",
  "status_code": 200,
  "message": "SELECT 1",
  "affected_rows": 1,
  "output_text": "+--------+\n| name   |\n+--------+\n| name_1 |\n+--------+\n"
}
```

## 빌드와 실행

### 서버 빌드

```powershell
docker compose run --rm dev bash -lc "make server"
```

### 서버 실행

```powershell
docker compose up --build -d server
```

### 헬스체크

```powershell
docker compose exec server curl --fail --silent --show-error http://127.0.0.1:8080/health
```

### 쿼리 테스트

```powershell
docker compose exec server bash -lc "curl --fail --silent --show-error -H 'Content-Type: text/plain; charset=utf-8' --data 'SELECT name FROM student WHERE id = 1;' http://127.0.0.1:8080/query"
```

### 종료

```powershell
docker compose down
```

## 현재 동시성 모델
- listener는 연결을 받아 thread pool에 job을 넣는다
- worker는 `http_server_handle_client(...)`를 실행한다
- `/query` 요청은 worker 안에서 엔진 callback으로 이어진다
- SQL 문장을 기준으로 `SELECT`는 `READ`, 그 외는 `WRITE`로 분류한다
- 다만 현재 runtime은 thread pool을 `CONCURRENCY_LOCK_POLICY_SERIAL_ALL`로 초기화하므로 실제 DB 실행 구간은 read/write 구분 없이 직렬화된다

현재 의미:
- API 서버는 동시에 여러 요청을 받을 수 있다
- 요청 처리 파이프라인과 worker 동작은 병렬이다
- DB 실행은 무결성을 우선해 보수적으로 직렬화되어 있다

현재 의미하지 않는 것:
- read-read 실제 병렬 실행
- write-read 동시 실행
- DB 엔진 내부의 fine-grained lock

## 현재 검증 상태
- `week8-team7-db-api` Docker 테스트: `375/375` 통과
- `TEST-ENGINE-ADAPTER`: `38/38` 통과
- `TEST-HTTP-FUNCTIONAL`: `-ValidateOnly` 기준 10 케이스 검증 통과
- `TEST-CONCURRENCY`: self-test 통과
- Docker 기반 실제 서버에 대해 `TEST-CONCURRENCY` 기본 3케이스 통과
- `/health`, `/query` Docker runtime smoke test 통과
- `X-Debug-Sleep-Ms: 3000` 요청 실측 기준 약 `3.66초` 지연 적용 확인

## 현재 기준 디렉터리 책임
- 루트
  - 공통 문서 기준
  - Docker/Makefile/compose 관리
  - 전체 조립과 최종 검증
- `week8-team7-db-api`
  - SQL 엔진 본체
- `SERVER-CORE`
  - 엔진 실행 경계
- `SERVER-HTTP`
  - HTTP 계약과 라우팅
- `SERVER-CONCURRENCY`
  - queue/pool/lock manager
- `SERVER-RUNTIME`
  - socket listener와 runtime 조립
- `TEST-*`
  - 계약/기능/동시성/경계 검증

## 남은 작업과 제한사항
- 현재 동시성은 API 레벨 중심이며 DB 실행 병렬성까지는 열지 않았다
- `READERS_PARALLEL` 정책 전환은 아직 하지 않았다
- `shutdown-during-request`는 공식 graceful shutdown 제어면이 아직 제한적이다
- `X-Debug-Sleep-Ms`는 Postman 시연용 디버그 기능이며, API 서버 계층 동시성 시연용으로만 사용한다
- `X-Debug-Sleep-Ms`는 SQL 파서나 DB 로직을 바꾸지 않고 HTTP 계층에서만 유지한다
- `X-Debug-Sleep-Ms`를 기능 테스트나 데모 스크립트에 반영하는 작업은 아직 후속 과제로 남아 있다

## 기본 가정
- 루트 공용 파일은 루트에서만 관리한다
- API는 REST 리소스형이 아니라 SQL 실행형 엔드포인트를 기본으로 한다
- raw SQL body와 현재 JSON 응답 스키마를 공통 계약으로 본다
- `X-Debug-Sleep-Ms`는 기본 기능이 아니라 선택적 디버그/데모 헤더로 간주한다
- Windows 호스트에서도 표준 실행은 Docker 기준으로 맞춘다
