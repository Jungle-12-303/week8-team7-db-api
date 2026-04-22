# SERVER-HTTP Agent Brief

## 목적
- HTTP 요청 파싱, 라우팅, JSON 응답, 요청/응답 로그 계약을 담당한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 디렉터리 `agent.md`
4. 현재 디렉터리 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 의사결정
- 루트 `PLAN.md`의 `GET /health`, `POST /query` 계약을 따른다.
- 루트 `agent.md`의 기록 규칙과 검증 규칙을 따른다.

## 담당 범위
- `Content-Length` 기반 HTTP request completeness 판단과 parsing
- `GET /health`, `POST /query` 라우팅
- raw SQL body와 `Content-Type` 검증
- `X-Debug-Sleep-Ms` 헤더 파싱과 pre-query debug delay 적용
- HTTP response JSON 생성
- `Connection: close` 기반 connection lifecycle 관리
- request parse 직후 `요청 수신`, route 처리 후 `응답 완료` 로그 출력
- `http_route_request()`의 최종 HTTP status code를 바깥으로 전달해 응답 완료 로그에 남기는 정리

## 범위 제외
- SQL 엔진 직접 구현
- thread scheduling과 lock manager 구현
- 서버 프로세스 기동 스크립트 자체 구현

## 인터페이스
- 입력: 소켓으로 들어오는 HTTP/1.0, HTTP/1.1 단일 요청
- 출력: 상태 코드, 헤더, JSON 응답, stdout 로그
- `/health`: `GET` + 빈 body, 응답 필드 `ok`, `path`, `status_code`, `message`
- `/query`: `POST` + raw SQL body, 허용 `Content-Type`은 없음, `text/plain`, `application/sql`
- `/query` optional debug header: `X-Debug-Sleep-Ms: <0~10000>`
- `X-Debug-Sleep-Ms`는 기본 비활성이며 `/query` 라우팅 직후, `execute_query(...)` 호출 전에만 적용된다.
- `X-Debug-Sleep-Ms` 값이 잘못되면 HTTP `400` + `error_code: "invalid_header"`로 응답한다.
- `/query` 응답: 성공 시 `ok`, `path`, `status_code`, `message`, `affected_rows`, `output_text`, 실패 시 `error_code` 추가
- stdout 로그 형식: `[HTTP] | req_id=<n> | event=<요청 수신|응답 완료> | method=<...> | path=<...> | status=<...> | bytes=<...> | debug_sleep_ms=<...> |`
- runtime-backed 배포에서는 queue saturation/drain rejection이 `SERVER-RUNTIME`에서 먼저 처리되며 현재 계약은 `503 server_busy`, `503 server_stopping`이다.

## 파일 범위
- `SERVER-HTTP/include/**`
- `SERVER-HTTP/src/**`
- `SERVER-HTTP/work.md`
- `SERVER-HTTP/agent.md`

## 의존성
- `SERVER-CORE`
- `SERVER-CONCURRENCY`
- `SERVER-RUNTIME`

## 완료 조건
- `http_protocol.h`, `http_server.h` 공개 계약이 현재 구현 기준으로 정리된다.
- `/health`, `/query` 요청 흐름, body 정책, 오류 매핑이 루트 `PLAN.md`와 일치한다.
- `X-Debug-Sleep-Ms` 계약이 기본 비활성, HTTP 경계 제한, pre-execute 위치 기준으로 정리된다.
- request/response 로그가 req_id 기준 고정 포맷으로 남고 응답 완료 로그에 최종 status가 포함된다.
- runtime 경계의 overload/shutdown rejection 상태 코드가 `503` 기준으로 정리된다.

## 테스트 기준
- 정상 SQL 요청 시 live endpoint 기준 JSON 필드와 상태 코드가 맞는다.
- 잘못된 method/path/body/media type은 일관되게 4xx/5xx로 변환된다.
- `Connection: close`, raw SQL body, `Content-Length` 기반 단일 요청 정책이 유지된다.
- `X-Debug-Sleep-Ms`가 `0~10000ms` 범위에서만 동작하고 잘못된 값은 `400 invalid_header`로 변환된다.
- stdout 캡처 시 `[HTTP]`, `req_id`, `event`, `status=<code>` 필드가 고정 형식으로 남는다.
