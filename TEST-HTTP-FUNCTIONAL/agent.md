# TEST-HTTP-FUNCTIONAL Agent Brief

## 목적
- HTTP 기능 흐름과 JSON 응답 형식을 검증한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 `/health`, `/query` 요구를 따른다.
- 루트 `agent.md`의 API/계약 검증 규칙을 따른다.

## 담당 범위
- `/health`
- `/query`
- raw SQL body 정상/오류 요청
- 잘못된 method/path/body/media type
- `X-Debug-Sleep-Ms` 유효/무효 헤더
- JSON 응답 구조
- HTTP trace log 형식 검증

## 범위 제외
- 병렬성 검증
- 엔진 어댑터 내부 검증

## 인터페이스
- 입력: HTTP 요청
- 출력: 상태 코드와 JSON 응답
- `/health`: `GET` + 빈 body, 응답 필드 `ok`, `path`, `status_code`, `message`
- `/query`: `POST` + raw SQL text body, 허용 `Content-Type`은 없음, `text/plain`, `application/sql`
- `/query` SQL 실패 응답은 HTTP `400` + `ok: false` + `error_code: "query_failed"`를 사용한다.
- `/query` optional debug header: `X-Debug-Sleep-Ms: <0~10000>`
- 잘못된 `X-Debug-Sleep-Ms`는 HTTP `400` + `error_code: "invalid_header"`로 처리한다.
- live smoke 기본 러너는 `scripts/run-http-functional-tests.ps1`이며 direct HTTP와 `docker-compose-exec` transport를 지원한다.

## 소유 경로
- `TEST-HTTP-FUNCTIONAL/cases/**`
- `TEST-HTTP-FUNCTIONAL/scripts/**`

## 의존성
- `SERVER-HTTP`
- `SERVER-RUNTIME`

## 완료 조건
- `/health`, `/query`, debug header, method/path/body/media type 시나리오가 케이스 파일로 정의된다.
- live endpoint 실행 경로가 문서화되고 실제 응답 결과가 로컬 기록에 반영된다.
- HTTP trace log가 있으면 최소 `요청 수신`, `응답 완료`, `status=<code>` 형식을 검증한다.

## 테스트 기준
- 정상 응답과 오류 응답이 분리된다.
- JSON 필드가 일관된다.
- `X-Debug-Sleep-Ms` 정상 케이스는 `200`, 잘못된 값은 `400 invalid_header`로 고정된다.
- `run-http-functional-tests.ps1 -Transport docker-compose-exec` 경로로 live endpoint suite를 재실행할 수 있어야 한다.
- 요청 trace 로그에 `[HTTP]`, `req_id`, `event=요청 수신/응답 완료`, `status=...`가 남는지 확인한다.
