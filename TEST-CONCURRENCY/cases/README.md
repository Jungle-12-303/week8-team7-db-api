# TEST-CONCURRENCY cases

- `concurrent_select_student.json`: `SELECT` 묶음을 sequential/concurrent로 비교해 read-read 병렬 가능성 또는 self-test 기준 병렬 흔적을 확인한다.
- `concurrent_insert_student.json`: `INSERT` 묶음을 sequential/concurrent로 비교하고 후행 조회로 write 직렬화 결과를 확인한다.
- `mixed_select_insert_student.json`: `SELECT + INSERT` 혼합 요청 이후 삽입 결과가 정확히 1회만 보이는지 검증한다.
- `debug_sleep_overlap_select.json`: `X-Debug-Sleep-Ms`를 사용해 API 계층 delay overlap과 `max_parallel > 1`를 확인한다.

## 설계 원칙
- 케이스는 `/query`에 raw SQL text body를 보내는 현재 루트 계약을 기본값으로 둔다.
- 반복 실행 충돌을 피하려고 `{{run_id}}` 템플릿으로 삽입 이름을 매 실행마다 바꾼다.
- 실제 서버 응답 필드명이 다르면 `expect`와 `target.body_template`만 바꿔 재사용한다.
- 현재 runtime 기본 lock policy는 `serial_all`이라 실제 DB 실행은 직렬화될 수 있고, API 계층 overlap 시연은 `debug_sleep_overlap_select.json`로 분리한다.
