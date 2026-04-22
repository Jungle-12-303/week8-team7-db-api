# TEST-EDGE-FAILURE 진행 현황

## 진행중
- 현재 진행중인 로컬 작업 없음

## 업데이트 필요
- Linux/Docker live runtime로 `queue-overflow`, `worker-exhaustion`, `shutdown-during-request` 결과 JSON을 누적할 것
- 현재 즉시 범위는 테스트 러너 출력 개편이므로 edge/failure 트랙은 우선 실패 응답 JSON과 현재 `[HTTP]` 요청/응답 로그까지만 맞추고 DB/락/요약 로그 assertion은 후순위로 둔다
- `SERVER-CORE` 또는 `SERVER-RUNTIME`에 fault injection 지점이 생기면 HTTP `500` 고정 케이스를 추가할 것

## 완료
- 2026-04-22 KST
  - mock 서버 기준 8개 엣지/실패 케이스 PASS를 확인함
  - `SERVER-RUNTIME`의 signal 기반 shutdown drain 경로를 반영해 `shutdown-during-request` 하네스를 갱신함
  - 현재 런타임 계약에 맞춰 `queue-overflow`, `worker-exhaustion` 허용 상태 코드를 `200/503`으로 좁힘
  - `X-Debug-Sleep-Ms` 계약을 반영한 `invalid-debug-header` 케이스를 추가함
  - 현재 `SERVER-HTTP` 계약에 맞춰 empty-body/oversized-sql/invalid-sql 케이스의 허용 상태 코드를 `400`/`413`/`400`으로 좁힘
  - TEST-EDGE-FAILURE 템플릿 폴더와 기록 구조를 생성함
  - 엣지/실패 케이스 JSON 카탈로그를 추가함
  - `scripts/run_edge_failure_cases.py` 블랙박스 하네스를 추가함
  - `scripts/mock_edge_server.py` 로컬 검증용 mock 서버를 추가함
  - HTTP 오류 전환 정책 문서를 추가함
