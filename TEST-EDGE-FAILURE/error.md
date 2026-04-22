# TEST-EDGE-FAILURE 이슈

## 열린 이슈
- graceful shutdown을 외부에서 요청하는 공식 제어면이 아직 없어 `shutdown-during-request`는 managed subprocess stop/restart 기반 근사 검증이다.

## 해결됨
- 2026-04-22 KST
  - 현재 HTTP 계층이 이미 확정한 `400`/`413`/`400` 상태 코드 구간은 케이스 허용값을 최종 계약으로 좁힘
  - 엣지/실패 테스트 전용 폴더 구조를 분리함
  - 빈 body, 과대 SQL, 연결 중단, overload, shutdown 시나리오를 자동 실행할 하네스를 추가함
