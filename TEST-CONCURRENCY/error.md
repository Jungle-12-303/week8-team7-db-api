# TEST-CONCURRENCY 이슈

## 열린 이슈
- 현재 `INSERT` 직렬화 판정은 wall-clock 비율과 후행 조회 기반의 외부 관측치다.
- 서버가 lock trace나 worker trace를 노출하지 않으면 직렬화 여부를 직접 증명하기보다 강하게 추정하는 수준에 머문다.

## 해결됨
- 2026-04-22 KST
  - `/query` 응답 필드명이 예전 `output` 계약에 묶여 있던 문제를 `output_text` 기준으로 정리함
  - 동시성 테스트 전용 폴더 구조를 분리함
  - 케이스 JSON과 실행 하네스를 추가해 병렬성 검증 기준을 코드로 고정함
