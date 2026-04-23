# 동시성 검증 사용법

이 저장소의 동시성 검증 하네스는 `TEST-CONCURRENCY/scripts/run_concurrency_case.py`입니다.
권장 실행 경로는 Docker로 서버를 띄운 뒤, 같은 Docker 네트워크 안에서 하네스를 실행하는 방식입니다.

## 1. 서버 실행

루트에서 서버 컨테이너를 먼저 올립니다.

```powershell
docker compose up -d server
```

정상 확인:

```powershell
docker compose logs --no-color --tail 50 server
docker compose exec server curl --fail --silent --show-error http://127.0.0.1:8080/health
```

정상 응답 예시:

```json
{"ok":true,"path":"/health","status_code":200,"message":"server is healthy"}
```

## 2. 전체 동시성 케이스 실행

아래 명령으로 기본 제공 케이스를 모두 실행할 수 있습니다.

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_select_student.json TEST-CONCURRENCY/cases/concurrent_insert_student.json TEST-CONCURRENCY/cases/mixed_select_insert_student.json TEST-CONCURRENCY/cases/debug_sleep_overlap_select.json --base-url http://server:8080 --timeout 15"
```

검증 대상:

- `concurrent_select_student.json`: 같은 종류의 `SELECT` 요청을 순차/동시로 비교
- `concurrent_insert_student.json`: `INSERT` 요청의 직렬화와 후행 조회 결과 확인
- `mixed_select_insert_student.json`: `SELECT + INSERT` 혼합 요청 후 삽입 결과 확인
- `debug_sleep_overlap_select.json`: `X-Debug-Sleep-Ms`를 사용해 API 계층 overlap과 `max_parallel > 1` 시연

참고:

- `INSERT` 전용 `X-Debug-Sleep-Ms` overlap 케이스는 아직 공식 테스트 자산으로 추가하지 않았습니다.
- 필요하면 [TEST-CONCURRENCY/work.md](/C:/Users/gi676/OneDrive/바탕 화면/dddddddd/TEST-CONCURRENCY/work.md:1)의 `업데이트 필요` 항목 기준으로 후속 추가합니다.

## 3. 개별 케이스 실행

`SELECT` 케이스만 실행:

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_select_student.json --base-url http://server:8080 --timeout 15"
```

`INSERT` 케이스만 실행:

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_insert_student.json --base-url http://server:8080 --timeout 15"
```

debug sleep overlap 케이스만 실행:

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/debug_sleep_overlap_select.json --base-url http://server:8080 --timeout 15"
```

## 4. 로컬에서 직접 실행

호스트 셸에서 직접 붙이고 싶으면 `DB_SERVER_URL` 환경 변수를 써도 됩니다.

```powershell
$env:DB_SERVER_URL = "http://127.0.0.1:8080"
python TEST-CONCURRENCY\scripts\run_concurrency_case.py TEST-CONCURRENCY\cases\concurrent_select_student.json
```

다만 환경에 따라 호스트의 `127.0.0.1:8080` 접근이 timeout 나는 경우가 있으니, 재현성과 안정성은 Docker 내부 실행 쪽이 더 좋습니다.

## 5. self-test

실제 서버 없이 하네스 자체만 검증하려면 내장 mock 서버로 self-test를 돌릴 수 있습니다.

```powershell
python TEST-CONCURRENCY\scripts\run_concurrency_case.py --self-test
```

이 모드는 하네스, 케이스 파일, assertion 로직이 기본적으로 정상인지 확인하는 용도입니다.

## 6. 자주 쓰는 옵션

- `--base-url`: 대상 서버 주소 지정
- `--timeout`: 요청별 타임아웃 지정
- `--skip-health-check`: 시작 전 `/health` 확인 생략
- `--output-json <path>`: 결과를 JSON 파일로 저장
- `--print-response-bodies`: 각 응답 body를 콘솔에 함께 출력

예시:

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_select_student.json --base-url http://server:8080 --timeout 20 --output-json /workspace/build/concurrency-report.json --print-response-bodies"
```

## 7. 결과 해석

출력은 케이스 단위로 `PASS` 또는 `FAIL`이 표시됩니다.

- `phase ... [sequential]`: 순차 실행 기준 시간
- `phase ... [concurrent]`: 동시 실행 시간
- `max_parallel`: 실제 요청 시간이 겹친 최대 개수
- `assertion ...`: 케이스가 기대한 조건 충족 여부
- `timing: start_spread_ms=...`: concurrent phase 요청들이 시작된 시간 범위
- `timing: overlap_window_ms=...`: 모든 요청이 동시에 in-flight였던 겹침 구간
- `timing: concurrency_confirmed=True`: overlap 시연 조건이 직접 충족됐다는 요약
- `timeline:`: 요청별 상대 시각 START/END 행

예를 들어 `concurrent-select`에서:

- `max_parallel=4`이면 4개 요청이 겹쳐 들어간 것은 확인된 상태입니다.
- 하지만 concurrent 시간이 sequential보다 항상 짧아야 하는 것은 아닙니다.

현재 assertion은 "더 빨랐는지" 또는 "겹쳐서 실행됐는지" 중 하나를 만족하면 통과하도록 잡혀 있습니다.

`debug_sleep_overlap_select`는 여기에 더해 `start_spread_at_most` assertion을 사용합니다.
즉 `max_parallel > 1`뿐 아니라 요청들이 "거의 동시에 START했다"는 조건이 결과에 직접 표시됩니다.

## 8. 현재 구현 기준 주의사항

현재 런타임은 `SERVER-RUNTIME/src/db_server_runtime.c`에서 `CONCURRENCY_LOCK_POLICY_SERIAL_ALL`을 기본 정책으로 사용합니다.
즉, worker 여러 개가 떠 있어도 실제 DB 실행 구간은 보수적으로 직렬화됩니다.

그래서 현재 동시성 검증의 의미는 다음에 가깝습니다.

- 서버가 동시 요청을 받아 큐잉하고 처리하는지
- 요청/응답 계약이 깨지지 않는지
- write 이후 결과 무결성이 유지되는지

반대로 아래는 현재 기본 설정만으로는 강하게 증명하지 못합니다.

- read-read 실제 병렬 실행에 따른 확실한 속도 향상
- write와 read가 lock 분리 정책 하에서 동시에 진행되는지

이 수준까지 검증하려면:

1. 런타임 lock policy를 `READERS_PARALLEL`로 변경
2. `concurrent_select_student.json` assertion을 더 엄격하게 조정

## 9. 지연시간 테스트

`X-Debug-Sleep-Ms`는 현재 구현되어 있습니다.
이 헤더는 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 적용되며, Postman이나 `curl`로 API 계층 동시 요청 수용을 시연할 때 사용합니다.

### Postman 요청 설정

- Method: `POST`
- URL: `http://localhost:8080/query`
- Headers:
  - `Content-Type: text/plain; charset=utf-8`
  - `X-Debug-Sleep-Ms: 3000`
- Body:

```sql
SELECT name FROM student WHERE id = 1;
```

### 한 줄 `curl`

```powershell
curl -H "Content-Type: text/plain; charset=utf-8" -H "X-Debug-Sleep-Ms: 3000" --data-binary "SELECT name FROM student WHERE id = 1;" http://localhost:8080/query
```

### 시연 절차

1. 서버를 실행합니다.

```powershell
docker compose up -d server
```

2. 로그를 같이 봅니다.

```powershell
docker compose logs -f server
```

3. Postman 탭을 3개 이상 열고 같은 요청을 복제합니다.
4. 각 탭에 `X-Debug-Sleep-Ms: 3000` 헤더를 넣습니다.
5. 거의 동시에 `Send`를 누릅니다.

### 기대 결과

- 스레드풀이 있고 sleep이 HTTP 계층에서 적용되면 여러 요청이 비슷한 시점에 시작되고, 응답도 대체로 3초 근처에서 몰려서 돌아옵니다.
- 로그에서는 `req_id`가 다른 여러 요청의 `요청 수신`, `응답 완료`가 연속해서 보입니다.
- 현재 런타임은 `SERIAL_ALL`이므로 DB 실행 자체의 병렬 속도 향상을 보여주는 것은 아닙니다.
- `run_concurrency_case.py`를 쓰는 경우에는 서버 로그 없이도 `start_spread_ms`, `overlap_window_ms`, `concurrency_confirmed`, START/END timeline만 보고 overlap을 설명할 수 있습니다.

### 주의사항

- 이 테스트는 DB 엔진 병렬성 시연이 아니라 API 서버 계층의 동시 요청 수용 시연입니다.
- sleep은 DB lock 안이 아니라 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 들어갑니다.
- `X-Debug-Sleep-Ms` 허용 범위는 `0~10000ms`입니다.
- 잘못된 값은 `400 invalid_header`로 응답합니다.

## 10. 로그 확인

현재 구현된 요청 추적/런타임/락 로그는 `stdout`으로 남습니다.
Docker 기준 확인 명령은 다음과 같습니다.

```powershell
docker compose logs --no-color --tail 50 server
```

현재 확인된 로그 계열:

```text
[HTTP] | req_id=12 | event=요청 수신 | method=POST | path=/query | ...
[HTTP] | req_id=12 | event=응답 완료 | method=POST | path=/query | status=200 | ...
[RUNTIME] | req_id=12 | event=스레드 할당 | thread=0 | ...
[RUNTIME] | req_id=12 | event=작업 종료 | thread=0 | result=ok | ...
[LOCK] | event=락 획득 | worker=0 | requested=read | effective=write | active_readers=0 | waiting_writers=0 | writer_active=1 |
[LOCK] | event=락 해제 | worker=0 | requested=read | effective=write | active_readers=0 | waiting_writers=0 | writer_active=0 |
```

현재 로그에서 아직 없는 것은 아래입니다.

- `DB 작업 시작`
- `DB 작업 종료`
- shutdown 요약 로그

## 11. 종료

검증이 끝나면 서버를 내립니다.

```powershell
docker compose down
```
