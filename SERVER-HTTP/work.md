# SERVER-HTTP 진행 현황

## 진행중
- live endpoint 결과 기준으로 `TEST-EDGE-FAILURE`의 `queue-overflow`, `worker-exhaustion`, `shutdown-during-request` 케이스를 계속 추적
- stdout 캡처 기준으로 `[요청 수신]`, `[응답 완료]` 로그 검증을 `TEST-HTTP-FUNCTIONAL`, `TEST-EDGE-FAILURE` 러너에 연결

## 업데이트 필요
- `SERVER-RUNTIME` live 환경에서 overload/shutdown rejection 응답과 로그를 한 번 더 캡처해 edge failure 기대값을 고정
- stdout 캡처 검증이 붙으면 req_id, `status=<code>`, `debug_sleep_ms=<value>` 필드를 테스트 기대값에 반영

## 완료
- 2026-04-22 KST
  - `SERVER-CONCURRENCY`, `SERVER-RUNTIME` 경계의 overload/queue saturation 응답을 현재 구현 기준 `503` 계열로 정리했다.
  - runtime-backed 배포 기준 queue full은 `503 server_busy`, queue closed/drain은 `503 server_stopping`으로 본다는 점을 `src/README.md`에 반영했다.
  - `/query` optional debug header `X-Debug-Sleep-Ms` 계약을 구현했다.
  - `X-Debug-Sleep-Ms`는 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 적용되도록 고정했다.
  - `X-Debug-Sleep-Ms` 범위를 `0~10000ms`로 제한하고, 잘못된 값은 `400 invalid_header`로 응답하도록 정리했다.
  - request parse 직후 `요청 수신`, route 처리 후 `응답 완료`를 req_id 기준 한 줄 로그로 남기도록 `http_server_handle_client()`를 정리했다.
  - `http_route_request()` 경로의 최종 HTTP status code를 바깥으로 전달하도록 정리해 응답 완료 로그에 `status=<code>`가 남도록 반영했다.
  - `TEST-HTTP-FUNCTIONAL` 쪽 `X-Debug-Sleep-Ms` 유효/무효 케이스와 README 계약이 현재 구현 기준으로 정렬된 상태를 확인했다.
  - `TEST-EDGE-FAILURE` 쪽 overload 상태 코드 케이스가 `503` 기준으로 정렬된 상태를 확인했다.
  - Docker 컨테이너에서 `make clean && make server` 컴파일을 통과시켜 `SERVER-HTTP` 변경분을 확인했다.
  - `X-Debug-Sleep-Ms: 3000` 요청 실측 결과 약 `3.66초` 지연이 적용됨을 확인했다.
  - Docker 기반 `db_server` 환경에서 `/health`, `/query` 기본 응답을 확인했다.
  - `http_server_handle_client()`가 실제 worker 기반 경로와 연결됨을 확인했다.
  - HTTP parser, router, JSON response 계약 구현을 완료했다.
  - 서버 핸들러 진입점 wiring을 완료했다.
  - SERVER-HTTP 템플릿 헤더와 기록 구조를 생성했다.
  - `/query` 본문 형식을 raw SQL text로 고정했다.
  - `/health`, `/query`, 오류 응답 JSON 스키마를 구현했다.
