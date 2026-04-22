# TEST-CONCURRENCY 컨텍스트

## 현재 상태
- 병렬 SQL 검증용 케이스와 실행 하네스가 준비됐다.
- 기본 검증 단위는 `cases/*.json`이며, 실행기는 `scripts/run_concurrency_case.py`다.
- self-test mock 서버와 케이스 기대값은 현재 `SERVER-HTTP`의 raw SQL/body + `message`/`affected_rows`/`output_text` 계약에 맞춰 정리됐다.

## 최근 결정
- 병렬 `SELECT`는 sequential 대비 wall-clock 단축 또는 동시 실행 흔적으로 판정한다.
- 직렬 `INSERT`는 concurrent 실행이 과도하게 빨라지지 않는지와 후행 `SELECT` 일관성으로 판정한다.
- 반복 실행 충돌을 피하려고 삽입 이름은 `{{run_id}}` 템플릿으로 생성한다.

## 다음 작업
- 루트 실행 경로가 정해지면 통합 명령으로 이 하네스를 연결한다.
