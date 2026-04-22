# TEST-EDGE-FAILURE 이슈

## 열린 이슈
- Windows 호스트에서는 live `db_server` 바이너리를 직접 실행할 수 없어 overload/shutdown 결과 누적은 Linux/Docker 실행 경로가 필요하다.

## 해결됨
- 2026-04-22 KST
  - signal 기반 graceful shutdown 경로를 반영해 `shutdown-during-request`를 더 정확한 drain 검증 형태로 정리함
  - `invalid-debug-header` 케이스와 현재 `503` overload 계약을 반영함
  - 현재 HTTP 계층이 이미 확정한 `400`/`413`/`400` 상태 코드 구간은 케이스 허용값을 최종 계약으로 좁힘
  - 엣지/실패 테스트 전용 폴더 구조를 분리함
  - 빈 body, 과대 SQL, 연결 중단, overload, shutdown 시나리오를 자동 실행할 하네스를 추가함
