# TEST-ENGINE-ADAPTER scripts

- `adapter_contract_test.c`
  - `week8-team7-db-api`의 lexer/parser/executor를 직접 호출해 어댑터 계약을 검증합니다.
  - `SERVER-CORE` 공개 헤더가 생기기 전까지는 이 파일 안의 adapter-like helper가 기준 계약 역할을 합니다.
- `run_adapter_contract_tests.ps1`
  - 위 테스트를 `gcc`로 빌드하고 실행합니다.
  - 실행 위치는 `TEST-ENGINE-ADAPTER` 루트 기준입니다.

## 실행
```powershell
.\scripts\run_adapter_contract_tests.ps1
```

## 검증 범위
- 정상 `SELECT` 결과 구조체 검증
- `WHERE id = ...` 결과 구조체 및 출력 캡처 검증
- 정상 `INSERT` 결과 구조체 검증
- parse 오류 전달 검증
- execute 오류 전달 검증
