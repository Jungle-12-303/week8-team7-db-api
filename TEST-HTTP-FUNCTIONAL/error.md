# TEST-HTTP-FUNCTIONAL 이슈

## 열린 이슈
- Windows 호스트 PowerShell에서 `http://127.0.0.1:8080` direct 접근이 timeout 나는 현상이 있다.
  - 우회: `run-http-functional-tests.ps1 -Transport docker-compose-exec`
  - 영향: live endpoint 검증은 가능하지만 direct transport 결과는 현재 환경에서 신뢰 기준으로 쓰지 않는다.

## 해결됨
- 2026-04-22 KST
  - `docker-compose-exec` transport를 추가해 호스트 direct timeout과 분리된 live HTTP smoke 경로를 확보함
  - `X-Debug-Sleep-Ms` 유효/무효 헤더 응답을 실제 서버 기준으로 고정함
  - 기능 테스트가 예전 JSON body/HTTP 200 SQL 실패 계약을 가정하던 문제를 현재 구현 기준으로 정리함
  - HTTP 기능 테스트 전용 폴더 구조를 분리함
