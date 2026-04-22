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
- HTTP request parsing
- HTTP response JSON 생성
- connection lifecycle 관리
- 엔드포인트 분기

## 범위 제외
- SQL 엔진 직접 호출 세부 구현
- thread scheduling과 lock manager
- 서버 프로세스 기동 설정

## 인터페이스
- 입력: 소켓으로 들어온 HTTP 요청
- 출력: 상태 코드, 헤더, JSON 응답
- 엔진 호출은 `SERVER-CORE` 인터페이스를 통해서만 수행한다.

## 소유 경로
- `SERVER-HTTP/include/**`
- `SERVER-HTTP/src/**`

## 의존성
- `SERVER-CORE`
- `SERVER-CONCURRENCY`

## 완료 조건
- `/health`, `/query` 요청 흐름이 문서로 정리된다.
- 잘못된 method/path/body에 대한 응답 정책이 정리된다.

## 테스트 기준
- 정상 SQL 요청 시 일관된 JSON 응답 형식이 정의된다.
- 잘못된 요청이 안전하게 4xx/5xx로 변환된다.
