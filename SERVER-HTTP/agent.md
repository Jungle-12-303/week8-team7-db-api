# SERVER-HTTP Agent Brief

## 목적
- HTTP 요청과 응답을 처리하는 서버 프로토콜 계층을 담당한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 `GET /health`, `POST /query` 요구를 따른다.
- 루트 `agent.md`의 기록 규칙과 검증 규칙을 따른다.

## 담당 범위
- `Content-Length` 기반 HTTP request completeness 판단과 parsing
- `GET /health`, `POST /query` 엔드포인트 분기
- raw SQL body 및 `Content-Type` 검증
- HTTP response JSON 생성
- `Connection: close` 기반 connection lifecycle 관리
- 필요 시 debug/demo용 pre-query HTTP 헤더 정책 정리

## 범위 제외
- SQL 엔진 직접 호출 세부 구현
- thread scheduling과 lock manager
- 서버 프로세스 기동 설정

## 인터페이스
- 입력: 소켓으로 들어온 HTTP/1.0, HTTP/1.1 단일 요청
- 출력: 상태 코드, 헤더, JSON 응답
- `/health`: `GET` + 빈 body, 응답 필드 `ok`, `path`, `status_code`, `message`
- `/query`: `POST` + raw SQL body, 허용 `Content-Type`은 없음, `text/plain`, `application/sql`
- `/query` 응답: 성공 시 `ok`, `path`, `status_code`, `message`, `affected_rows`, `output_text`, 실패 시 `error_code` 추가
- 엔진 호출은 `SERVER-CORE` 인터페이스와 query executor callback 계약을 통해서만 수행한다.

## 소유 경로
- `SERVER-HTTP/include/**`
- `SERVER-HTTP/src/**`

## 의존성
- `SERVER-CORE`
- `SERVER-CONCURRENCY`

## 완료 조건
- `http_protocol.h`, `http_server.h` 공개 계약이 현재 구현 기준으로 정리된다.
- `/health`, `/query` 요청 흐름, body 정책, 오류 매핑이 루트 `PLAN.md`와 일치한다.
- debug/demo용 지연 헤더를 도입할 경우 기본 비활성, HTTP 계층 한정, pre-execute 위치가 문서로 고정된다.

## 테스트 기준
- 정상 SQL 요청 시 live endpoint 기준 JSON 필드와 상태 코드가 일관된다.
- 잘못된 method/path/body/media type이 안전하게 4xx/5xx로 변환된다.
- `Connection: close`, raw SQL body, `Content-Length` 기반 단일 요청 정책이 유지된다.
