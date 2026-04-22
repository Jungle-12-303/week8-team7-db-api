# TEST-ENGINE-ADAPTER 컨텍스트

## 현재 상태
- 엔진 어댑터 검증 전용 폴더다.
- `cases/*.md`에 정상/오류 SQL 계약 케이스를 정의했다.
- `scripts/adapter_contract_test.c`는 `SERVER-CORE` 공개 API를 호출해 `ok`, `affected_rows`, `message`, `output_text` 계약을 검증한다.
- `.\scripts\run_adapter_contract_tests.ps1` 실행 기준으로 38개 검증이 통과했다.

## 다음 작업
- 루트 또는 통합 단계에서 이 테스트를 공용 빌드 경로에 연결한다.
