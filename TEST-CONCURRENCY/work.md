# TEST-CONCURRENCY 진행 현황

## 진행중
- raw SQL body와 현재 `/query` 응답 스키마 기준 self-test 계약 정렬 완료
- 실제 통합 서버에 연결할 루트 실행 경로 대기

## 업데이트 필요
- `Makefile` 또는 통합 실행 스크립트가 생기면 `scripts/run_concurrency_case.py` 호출 경로를 루트 기준으로 연결할 것
- 실제 서버에서 `serial_all` 기본 정책 기준 end-to-end 결과를 기록할 것

## 완료
- 2026-04-22 KST
  - current `SERVER-HTTP` 계약에 맞춰 `output_text`, `message`, `affected_rows` 기준으로 케이스 기대값을 조정함
  - raw SQL text body 기준으로 mock 서버와 self-test 경로를 정리하고 재통과를 확인함
  - TEST-CONCURRENCY 템플릿 폴더와 기록 구조를 생성함
  - 병렬 `SELECT`, 직렬 `INSERT`, 혼합 `SELECT + INSERT` 케이스 JSON을 추가함
  - wall-clock 비교와 응답 일관성 검증을 수행하는 `scripts/run_concurrency_case.py`를 추가함
  - 내장 mock 서버 기반 `--self-test` 경로를 추가함
