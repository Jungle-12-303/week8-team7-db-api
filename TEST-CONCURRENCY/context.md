# TEST-CONCURRENCY 컨텍스트

## 현재 상태
- 병렬 SQL 검증용 케이스와 실행 하네스가 준비됐다.
- 기본 검증 단위는 `cases/*.json`이며, 실행기는 `scripts/run_concurrency_case.py`다.
- self-test mock 서버와 케이스 기대값은 현재 `SERVER-HTTP`의 raw SQL/body + `message`/`affected_rows`/`output_text` 계약에 맞춰 정리됐다.
- `X-Debug-Sleep-Ms`를 이용한 API 계층 overlap 시연 케이스가 추가됐다.
- Docker 기반 실제 서버 against 기본 4케이스 재실행까지 완료됐다.
- 러너 콘솔만으로도 phase timing summary와 START/END timeline을 읽어 overlap을 설명할 수 있다.

## 최근 결정
- 병렬 `SELECT`는 sequential 대비 wall-clock 단축 또는 동시 실행 흔적으로 판정한다.
- 직렬 `INSERT`는 concurrent 실행이 과도하게 빨라지지 않는지와 후행 `SELECT` 일관성으로 판정한다.
- 반복 실행 충돌을 피하려고 삽입 이름은 `{{run_id}}` 템플릿으로 생성한다.
- 현재 runtime 기본 정책은 `serial_all`이므로 실제 DB 실행 병렬성과 API 계층 overlap 시연을 서로 다른 케이스로 본다.

## 다음 작업
- 실시간 trace/log 기능이 붙으면 `stdout` 또는 `docker compose logs` 기준 검증 절차를 케이스 운용 문서에 반영한다.
