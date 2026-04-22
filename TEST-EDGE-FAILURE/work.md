# TEST-EDGE-FAILURE 진행 현황

## 진행중
- graceful shutdown 공식 제어면이 없어 `shutdown-during-request`는 근사 검증 상태다

## 업데이트 필요
- `SERVER-RUNTIME`에 graceful shutdown 제어면이 추가되면 `shutdown-during-request` 케이스를 drain 검증으로 강화할 것
- overload/worker exhaustion 케이스는 실제 런타임 조립 후 결과를 누적할 것

## 완료
- 2026-04-22 KST
  - 현재 `SERVER-HTTP` 계약에 맞춰 empty-body/oversized-sql/invalid-sql 케이스의 허용 상태 코드를 `400`/`413`/`400`으로 좁힘
  - TEST-EDGE-FAILURE 템플릿 폴더와 기록 구조를 생성함
  - 엣지/실패 케이스 JSON 카탈로그를 추가함
  - `scripts/run_edge_failure_cases.py` 블랙박스 하네스를 추가함
  - `scripts/mock_edge_server.py` 로컬 검증용 mock 서버를 추가함
  - HTTP 오류 전환 정책 문서를 추가함
