# TEST-ENGINE-ADAPTER cases

- 엔진 어댑터가 보장해야 하는 입력/출력 계약을 케이스별로 정의합니다.
- 각 문서는 SQL, 최소 fixture, 기대 결과 구조체, 기대 출력 캡처 규칙을 함께 적습니다.
- 현재 정의된 케이스:
  - `select_general_where_ok.md`
  - `select_id_ok.md`
  - `insert_ok.md`
  - `parse_error.md`
  - `execute_error.md`
