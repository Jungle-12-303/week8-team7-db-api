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

아래 명령으로 기본 제공 케이스 3개를 모두 실행할 수 있습니다.

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_select_student.json TEST-CONCURRENCY/cases/concurrent_insert_student.json TEST-CONCURRENCY/cases/mixed_select_insert_student.json --base-url http://server:8080 --timeout 15"
```

검증 대상:

- `concurrent_select_student.json`: 같은 종류의 `SELECT` 요청을 순차/동시로 비교
- `concurrent_insert_student.json`: `INSERT` 요청의 직렬화와 후행 조회 결과 확인
- `mixed_select_insert_student.json`: `SELECT + INSERT` 혼합 요청 후 삽입 결과 확인

## 3. 개별 케이스 실행

`SELECT` 케이스만 실행:

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_select_student.json --base-url http://server:8080 --timeout 15"
```

`INSERT` 케이스만 실행:

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_insert_student.json --base-url http://server:8080 --timeout 15"
```

혼합 케이스만 실행:

```powershell
docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/mixed_select_insert_student.json --base-url http://server:8080 --timeout 15"
```

## 4. 로컬에서 직접 실행

호스트 셸에서 직접 붙이고 싶으면 `DB_SERVER_URL` 환경 변수를 써도 됩니다.

```powershell
$env:DB_SERVER_URL = "http://127.0.0.1:8080"
python TEST-CONCURRENCY\scripts\run_concurrency_case.py TEST-CONCURRENCY\cases\concurrent_select_student.json
```

다만 환경에 따라 호스트의 `127.0.0.1:8080` 접근이 막히는 경우가 있으니, 재현성과 안정성은 Docker 내부 실행 쪽이 더 좋습니다.

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

예를 들어 `concurrent-select`에서:

- `max_parallel=4`이면 4개 요청이 겹쳐 들어간 것은 확인된 상태입니다.
- 하지만 concurrent 시간이 sequential보다 항상 짧아야 하는 것은 아닙니다.

현재 assertion은 "더 빨랐는지" 또는 "겹쳐서 실행됐는지" 중 하나를 만족하면 통과하도록 잡혀 있습니다.

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

Postman으로 API 서버의 동시 요청 수용을 시연하려면 `X-Debug-Sleep-Ms` 같은 debug delay 헤더 기능이 먼저 구현되어 있어야 합니다.

현재 저장소 기준으로 이 기능은 `SERVER-HTTP/work.md`에 작업 항목으로 기록되어 있고, 코드에는 아직 반영되지 않았을 수 있습니다.
미구현 상태라면 아래 절차는 바로 사용할 수 없습니다.

기능이 구현된 뒤에는 아래처럼 테스트합니다.

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

### 시연 절차

1. 서버를 실행합니다.

```powershell
docker compose up -d server
```

2. Postman 탭을 3개 이상 열고 같은 요청을 복제합니다.
3. 각 탭에 `X-Debug-Sleep-Ms: 3000` 헤더를 넣습니다.
4. 거의 동시에 `Send`를 누릅니다.

### 기대 결과

- 스레드풀이 있고 sleep이 HTTP 계층에서 적용되면 여러 요청이 비슷한 시점에 시작되고, 응답도 대체로 3초 근처에서 몰려서 돌아옵니다.
- 요청이 직렬로만 처리되면 응답이 3초, 6초, 9초처럼 순차적으로 밀릴 수 있습니다.

### 주의사항

- 이 테스트는 DB 엔진 병렬성 시연이 아니라 API 서버 계층의 동시 요청 수용 시연입니다.
- sleep은 DB lock 안이 아니라 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 들어가야 합니다.
- 현재 런타임은 `CONCURRENCY_LOCK_POLICY_SERIAL_ALL`이므로 실제 SQL 실행은 여전히 직렬화될 수 있습니다.

## 10. 종료

검증이 끝나면 서버를 내립니다.

```powershell
docker compose down
```
