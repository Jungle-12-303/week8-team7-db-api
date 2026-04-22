# TEST-CONCURRENCY cases

- `concurrent_select_student.json`: `SELECT` 묶음을 sequential/concurrent로 비교해 read-read 병렬성을 확인한다.
- `concurrent_insert_student.json`: `INSERT` 묶음을 sequential/concurrent로 비교하고 후행 조회로 write 직렬화 결과를 확인한다.
- `mixed_select_insert_student.json`: `SELECT + INSERT` 혼합 요청 이후 삽입 결과가 정확히 1회만 보이는지 검증한다.

## 설계 원칙
- 케이스는 `/query`에 SQL 문자열을 JSON으로 보내는 MVP 계약을 기본값으로 둔다.
- 반복 실행 충돌을 피하려고 `{{run_id}}` 템플릿으로 삽입 이름을 매 실행마다 바꾼다.
- 실제 서버 응답 필드명이 다르면 `expect`와 `target.body_template`만 바꿔 재사용한다.
