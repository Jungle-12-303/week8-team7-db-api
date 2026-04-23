# 미니 DBMS API 서버 루트 계획

## Summary
- 기존 SQL 엔진은 `week8-team7-db-api`를 그대로 사용한다.
- 루트 런타임은 `SERVER-CORE`, `SERVER-HTTP`, `SERVER-CONCURRENCY`, `SERVER-RUNTIME`를 조립해 `build/bin/db_server`를 만든다.
- 현재 공개 HTTP API는 `GET /health`, `POST /query` 두 개다.
- `POST /query`는 raw SQL text body를 받아 엔진으로 넘기고 JSON으로 결과를 반환한다.
- 동시성은 API 서버 계층에서 구현되어 있다. 요청 수용, 큐잉, worker 배정은 병렬이지만 실제 DB 실행 구간은 현재 `CONCURRENCY_LOCK_POLICY_SERIAL_ALL` 정책으로 직렬화한다.
- `X-Debug-Sleep-Ms`는 API 계층 동시 요청 시연용 선택적 디버그 헤더다.
- 현재 즉시 시연 경로는 `TEST-CONCURRENCY` 러너 출력이며, 필요 시 `docker compose logs -f server`의 `[HTTP]`, `[RUNTIME]`, `[LOCK]` 로그를 보조 증거로 함께 사용할 수 있다.
- 현재 HTTP 계층 요청 추적 로그, runtime 작업 배정/종료 로그, lock wait/acquire/release 로그는 구현되어 있고, DB 작업 시작/종료 및 종료 요약 로그는 후속 작업으로 남아 있다.
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
- `X-Debug-Sleep-Ms` 처리
- 요청 수신/응답 완료 한 줄 추적 로그

### `SERVER-CONCURRENCY`
- bounded job queue
- worker thread pool
- lock manager
- worker 할당 콜백과 현재 worker index 노출 지점 제공
- `[LOCK]` 고정 형식 로그와 `requested/effective` 락 상태 전이 로그 제공

### `SERVER-RUNTIME`
- listener socket 생성
- accept loop
- thread pool submit
- HTTP와 engine callback wiring
- 실행 설정 로딩
- `[RUNTIME]` 기준 `스레드 할당`, `작업 종료` 로그 연결

### `TEST-*`
- `TEST-ENGINE-ADAPTER`: `SERVER-CORE` 공개 API 계약 검증
- `TEST-HTTP-FUNCTIONAL`: HTTP 계약과 live 응답 검증
- `TEST-CONCURRENCY`: 동시 요청, overlap, 결과 무결성 검증과 시연용 timing/timeline 출력
- `TEST-EDGE-FAILURE`: 경계 실패 케이스 검증

## API 명세

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
- queue full은 현재 `503 server_busy`, runtime drain/queue closed는 `503 server_stopping`으로 본다

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

## 로그와 관측

### 현재 구현됨
- `SERVER-HTTP`는 요청 parse 직후 `요청 수신`, route 처리 후 `응답 완료`를 req_id 기준 한 줄 로그로 남긴다
- `SERVER-RUNTIME`은 `assigned`/`notify` 콜백을 통해 req_id 기준 `스레드 할당`, `작업 종료` 로그를 남긴다
- `SERVER-CONCURRENCY`는 `[LOCK] | event=락 대기/락 획득/락 해제 | worker=... | requested=... | effective=... | active_readers=... | waiting_writers=... | writer_active=... |` 형식 로그를 남긴다
- 로그는 기본적으로 서버 `stdout`에 기록되며 Docker에서는 `docker compose logs -f server`로 확인한다
- 현재 확인된 형식은 다음 계열이다

```text
[HTTP] | req_id=12 | event=요청 수신 | method=POST | path=/query | ...
[HTTP] | req_id=12 | event=응답 완료 | method=POST | path=/query | status=200 | ...
[RUNTIME] | req_id=12 | event=스레드 할당 | thread=0 | ...
[LOCK] | event=락 획득 | worker=0 | requested=read | effective=write | active_readers=0 | waiting_writers=0 | writer_active=1 |
```

### 아직 남아 있음
- `DB 작업 시작`
- `DB 작업 종료`
- shutdown 시 마지막 요약 로그

## 동시성 시연 경로
- 현재 권장 시연은 `TEST-CONCURRENCY/scripts/run_concurrency_case.py` 출력이다.
- 특히 `debug_sleep_overlap_select.json`은 `X-Debug-Sleep-Ms: 500`을 사용해 API 계층 overlap을 눈으로 확인하도록 설계돼 있다.
- 현재 러너 출력은 phase별로 다음 정보를 직접 보여준다.
  - `start_spread_ms`
  - `overlap_window_ms`
  - `concurrency_confirmed`
  - 요청별 상대 시각 START/END timeline
- 따라서 동시성 시연 요구가 "테스트 실행 결과만 보고 확인"인 경우, 러너 출력만으로 설명 가능하다.
- 서버 로그를 같이 보면 `[HTTP]`, `[RUNTIME]`, `[LOCK]`를 통해 `thread=<worker_index>`, `requested/effective`, 락 대기 여부를 추가로 확인할 수 있다.
- `INSERT`용 `X-Debug-Sleep-Ms` overlap 전용 케이스는 아직 공식 테스트 자산으로 추가하지 않았고, 필요 시 `TEST-CONCURRENCY/work.md`의 `업데이트 필요` 항목으로 후속 관리한다.

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
docker compose exec server curl --fail --silent --show-error -H "Content-Type: text/plain; charset=utf-8" --data-binary "SELECT name FROM student WHERE id = 1;" http://127.0.0.1:8080/query
```

### 지연 시연

```powershell
curl -H "Content-Type: text/plain; charset=utf-8" -H "X-Debug-Sleep-Ms: 3000" --data-binary "SELECT name FROM student WHERE id = 1;" http://localhost:8080/query
```

### 동시성 시연 테스트

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/debug_sleep_overlap_select.json --base-url http://server:8080 --timeout 15"
```

### 로그 확인

```powershell
docker compose logs -f server
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
- `X-Debug-Sleep-Ms`로 API 계층 overlap을 시연할 수 있다
- DB 실행은 무결성을 우선해 보수적으로 직렬화되어 있다

현재 의미하지 않는 것:
- read-read 실제 병렬 실행
- write-read 동시 실행
- DB 엔진 내부의 fine-grained lock

## 현재 검증 상태
- `week8-team7-db-api` Docker 테스트: `375/375` 통과
- `TEST-ENGINE-ADAPTER`: `38/38` 통과
- `TEST-HTTP-FUNCTIONAL`
  - `-ValidateOnly` 기준 12 케이스 검증 통과
  - `-Transport docker-compose-exec` 기준 실제 서버 `12/12 passed`
- `TEST-CONCURRENCY`
  - self-test 통과
  - `run_concurrency_case.py` 출력에 `start_spread_ms`, `overlap_window_ms`, `concurrency_confirmed`, START/END timeline 추가 완료
  - `debug_sleep_overlap_select.json`에 `start_spread_at_most` assertion 추가 완료
  - live `debug_sleep_overlap_select` 재검증에서 `start_spread_at_most` 포함 PASS 기록
  - Docker 기반 실제 서버에서 `concurrent_select`, `concurrent_insert`, `mixed_select_insert`, `debug_sleep_overlap_select` 4케이스 통과
- `SERVER-RUNTIME` / `SERVER-CONCURRENCY`
  - Docker server logs 기준 `[RUNTIME]`의 `스레드 할당`, `작업 종료` 형식 확인
  - Docker server logs 기준 `[LOCK]`의 `락 대기`, `락 획득`, `락 해제`, `requested/effective` 필드 확인
  - `serial_all` 정책에서 `requested=read`, `effective=write` 승격이 실제 로그에 드러나는 것 확인
  - 단일 `SELECT`보다 `TEST-CONCURRENCY`의 `concurrent_insert` 또는 `debug_sleep_overlap_select`가 로그 시연 경로로 더 적합함을 문서 기준으로 정리
- `TEST-EDGE-FAILURE`
  - mock 서버 기준 8개 케이스 통과
  - `invalid-debug-header` 케이스 추가 및 상태 코드 정렬 완료
- `/health`, `/query` Docker runtime smoke test 통과
- `X-Debug-Sleep-Ms: 3000` 요청 실측 기준 약 `3.66초` 지연 적용 확인
- `X-Debug-Sleep-Ms: 300` live 기능 테스트 기준 약 `320ms` 지연 적용 확인
- `docker compose logs` 기준 HTTP 요청 추적 로그 `[요청 수신]`, `[응답 완료]`, `status=...` 형식 확인

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
  - HTTP 계약, 디버그 헤더, 요청 추적 로그
- `SERVER-CONCURRENCY`
  - queue/pool/lock manager
- `SERVER-RUNTIME`
  - socket listener와 runtime 조립
- `TEST-*`
  - 계약/기능/동시성/경계 검증

## 남은 작업과 제한사항
- 현재 동시성은 API 레벨 중심이며 DB 실행 병렬성까지는 열지 않았다
- `READERS_PARALLEL` 정책 전환은 아직 하지 않았다
- `DB 작업 시작/종료`와 shutdown 요약 로그는 아직 미구현이다
- 현재 동시성 시연 요구는 `TEST-CONCURRENCY` 출력으로 충족되고, 필요 시 `[HTTP]`, `[RUNTIME]`, `[LOCK]` 로그로 보조 설명할 수 있다
- `INSERT` 전용 debug sleep overlap 시연은 아직 계획 단계이며, 구현 전까지는 `TEST-CONCURRENCY/work.md`에만 작업 항목으로 남긴다
- `shutdown-during-request`는 공식 graceful shutdown 제어면이 아직 제한적이다
- `queue-overflow`, `worker-exhaustion`, `shutdown-during-request`의 live Docker 결과 누적은 아직 남아 있다
- Windows 호스트 PowerShell에서 `http://127.0.0.1:8080` direct 접근 timeout 원인은 아직 분리되지 않았다
- `X-Debug-Sleep-Ms`는 DB 병렬성 시연이 아니라 API 서버 계층 동시 요청 시연용이다
- `X-Debug-Sleep-Ms`는 SQL 파서나 DB 로직을 바꾸지 않고 HTTP 계층에서만 유지한다

## 기본 가정
- 루트 공용 파일은 루트에서만 관리한다
- API는 REST 리소스형이 아니라 SQL 실행형 엔드포인트를 기본으로 한다
- raw SQL body와 현재 JSON 응답 스키마를 공통 계약으로 본다
- `X-Debug-Sleep-Ms`는 선택적 디버그/데모 헤더로 간주한다
- Windows 호스트에서도 표준 실행은 Docker 기준으로 맞춘다
