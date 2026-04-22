# 미니 DBMS API 서버

## 발표 한 줄 요약
- 기존 SQL 엔진을 HTTP API 서버로 감싸고, thread pool 기반 동시 요청 처리와 API 계층 동시성 시연까지 연결한 미니 DBMS 서버입니다.

## 무엇을 만들었나
- `week8-team7-db-api`의 SQL 엔진을 재사용했습니다.
- 루트에서 `SERVER-CORE`, `SERVER-HTTP`, `SERVER-CONCURRENCY`, `SERVER-RUNTIME`를 조립해 `db_server`를 구동합니다.
- 공개 API는 최소 범위로 `GET /health`, `POST /query`를 제공합니다.
- `POST /query`는 raw SQL text body를 받아 엔진으로 실행하고 JSON으로 응답합니다.

## 구조
- `week8-team7-db-api`
  - lexer, parser, executor, CSV storage, index를 포함한 기존 엔진
- `SERVER-CORE`
  - 서버와 엔진 사이 실행 경계
- `SERVER-HTTP`
  - HTTP 파싱, 라우팅, JSON 응답, `X-Debug-Sleep-Ms`
- `SERVER-CONCURRENCY`
  - bounded queue, worker thread pool, lock manager
- `SERVER-RUNTIME`
  - socket listener, accept loop, thread pool submit, 서버 조립
- `TEST-*`
  - 어댑터, HTTP 기능, 동시성, 실패/엣지 테스트

## 핵심 포인트
- 동시성은 API 서버 계층에서 구현되어 있습니다.
- 요청 수용, 큐잉, worker 배정은 병렬입니다.
- 실제 DB 실행 구간은 현재 `SERIAL_ALL` 정책이라 직렬화됩니다.
- 그래서 지금 시연하는 것은 "DB 엔진 내부 병렬성"이 아니라 "API 계층 동시 요청 처리"입니다.

## API 계약

### `GET /health`
```json
{
  "ok": true,
  "path": "/health",
  "status_code": 200,
  "message": "server is healthy"
}
```

### `POST /query`
- 요청 body: raw SQL text
- 권장 헤더: `Content-Type: text/plain; charset=utf-8`
- 선택 헤더: `X-Debug-Sleep-Ms: <0~10000>`

성공 예시:

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

## 실행 방법

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

## 발표용 데모

### 1. 기본 API 확인
```powershell
docker compose up --build -d server
docker compose exec server curl --fail --silent --show-error http://127.0.0.1:8080/health
docker compose exec server bash -lc "curl --fail --silent --show-error -H 'Content-Type: text/plain; charset=utf-8' --data 'SELECT name FROM student WHERE id = 1;' http://127.0.0.1:8080/query"
```

### 2. 지연 요청 시연
```powershell
curl -H "Content-Type: text/plain; charset=utf-8" -H "X-Debug-Sleep-Ms: 3000" --data "SELECT name FROM student WHERE id = 1;" http://localhost:8080/query
```

### 3. 동시성 시연
```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/debug_sleep_overlap_select.json --base-url http://server:8080 --timeout 15"
```

이 명령에서 확인할 값:
- `max_parallel`
- `start_spread_ms`
- `overlap_window_ms`
- `concurrency_confirmed`
- `timeline`의 START/END 행

예시 해석:
- `max_parallel > 1`
- `concurrency_confirmed=True`
- START 3개가 짧은 시간 안에 몰려 있으면
  - API 계층에서 동시 요청이 실제로 겹쳐 처리된 것입니다.

## 발표 때 설명할 문장
- "이 서버는 요청을 thread pool에 넣어 동시에 처리합니다."
- "다만 현재는 DB 실행 구간을 보수적으로 직렬화해서 무결성을 우선합니다."
- "그래서 우리가 시연하는 동시성은 DB 내부 병렬 실행이 아니라 API 서버 계층 동시성입니다."
- "동시성 여부는 서버 내부 로그가 아니라 테스트 러너의 timeline 출력만으로도 바로 확인할 수 있습니다."

## 현재 확인된 상태
- `week8-team7-db-api` Docker 테스트 `375/375` 통과
- `TEST-ENGINE-ADAPTER` `38/38` 통과
- `TEST-HTTP-FUNCTIONAL` live `12/12 passed`
- `TEST-CONCURRENCY` 4케이스 통과
- `TEST-EDGE-FAILURE` mock 8케이스 통과
- `debug_sleep_overlap_select`는 `timeline`, `start_spread_ms`, `overlap_window_ms`, `concurrency_confirmed` 출력 지원

## 한계
- 현재 `READERS_PARALLEL`은 적용하지 않았습니다.
- DB 작업 시작/종료, 락 대기/획득/해제, 종료 요약 로그는 아직 미구현입니다.
- 따라서 현재 발표 포인트는 "API 계층 동시성 시연"까지입니다.

## 참고 문서
- [PLAN.md](./PLAN.md)
- [howtouse.md](./howtouse.md)
- [TEST-CONCURRENCY/work.md](./TEST-CONCURRENCY/work.md)
