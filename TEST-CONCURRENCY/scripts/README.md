# TEST-CONCURRENCY scripts

- `run_concurrency_case.py`는 동시성 케이스 JSON을 읽어 `/health`, `/query` 기반 wall-clock 비교와 일관성 검증을 수행한다.
- 기본 대상 서버 주소는 `DB_SERVER_URL` 환경 변수에서 읽고, 필요하면 `--base-url`로 덮어쓴다.
- `--self-test`를 사용하면 내장 mock HTTP 서버로 하네스와 케이스를 함께 검증한다.

## 사용 예시
- `python scripts/run_concurrency_case.py cases/concurrent_select_student.json --base-url http://127.0.0.1:8080`
- `$env:DB_SERVER_URL = "http://127.0.0.1:8080"; python scripts/run_concurrency_case.py cases/*.json`
- `python scripts/run_concurrency_case.py --self-test`

## 케이스 계약
- `target.body_template`로 raw SQL text body나 JSON body 포맷을 바꿀 수 있다.
- `expect.json_equals`, `expect.json_string_contains`, `expect.json_string_occurrence_counts`로 응답 검증을 세밀하게 조정할 수 있다.
- concurrent phase 결과에는 `start_spread_ms`, `overlap_window_ms`, `concurrency_confirmed`, START/END timeline이 함께 출력된다.
- 직렬 `INSERT` 판정은 wall-clock 비율과 후행 조회 일관성으로 확인한다.
- `X-Debug-Sleep-Ms` 같은 요청 헤더도 케이스별 `headers`로 직접 지정할 수 있다.
