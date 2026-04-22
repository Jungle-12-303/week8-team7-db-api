# TEST-HTTP-FUNCTIONAL 진행 현황

## 진행중
- raw SQL body와 현재 JSON 응답 스키마 기준으로 기능 케이스 정렬 완료

## 업데이트 필요
- 서버 기동 스크립트가 준비되면 실제 endpoint smoke 실행 결과 기록
- `SERVER-RUNTIME` 경로가 연결되면 live endpoint 결과를 누적

## 완료
- 2026-04-22 KST
  - `/query` 요청 계약을 JSON `{ "sql": ... }`에서 raw SQL text body로 정리함
  - `/health`, `/query`, method/path/body 오류 케이스를 현재 `SERVER-HTTP` 필드(`ok`, `path`, `status_code`, `error_code`, `message`, `affected_rows`, `output_text`) 기준으로 갱신함
  - `run-http-functional-tests.ps1 -ValidateOnly`로 10개 케이스 정의 검증을 다시 통과함
  - TEST-HTTP-FUNCTIONAL 템플릿 폴더와 기록 구조를 생성함
  - `/health`, `/query`, 잘못된 method/path/body 검증용 케이스 JSON을 추가함
  - `cases/*.json` 기반 PowerShell HTTP 기능 러너를 추가함
  - `/query` 요청/응답 로컬 계약을 문서로 고정함
