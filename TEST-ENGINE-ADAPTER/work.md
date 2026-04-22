# TEST-ENGINE-ADAPTER 진행 현황

## 진행중
- `SERVER-CORE` 공개 API 기준 contract regression을 유지 중

## 업데이트 필요
- 루트 빌드 또는 통합 테스트 진입점에 `run_adapter_contract_tests.ps1` 수준의 명령을 편입

## 완료
- 2026-04-22 KST
  - `scripts/adapter_contract_test.c`를 `SERVER-CORE` 공개 API 호출 기준으로 전환함
  - `.\scripts\run_adapter_contract_tests.ps1` 재실행으로 38개 검증 재통과를 확인함
  - TEST-ENGINE-ADAPTER 템플릿 폴더와 기록 구조를 생성함
  - 정상 `SELECT`, `WHERE id`, `INSERT`, parse 오류, execute 오류 케이스 문서를 추가함
  - 어댑터 결과 구조체와 출력 캡처를 검증하는 `adapter_contract_test.c`와 실행 스크립트를 추가함
