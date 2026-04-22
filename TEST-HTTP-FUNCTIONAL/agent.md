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
- 정상 SQL 요청
- 잘못된 method/path
- JSON 응답 구조

## 범위 제외
- 병렬성 검증
- 엔진 어댑터 내부 검증

## 인터페이스
- 입력: HTTP 요청
- 출력: 상태 코드와 JSON 응답

## 소유 경로
- `TEST-HTTP-FUNCTIONAL/cases/**`
- `TEST-HTTP-FUNCTIONAL/scripts/**`

## 의존성
- `SERVER-HTTP`
- `SERVER-RUNTIME`

## 완료 조건
- 최소 엔드포인트 기능 검증 시나리오가 정의된다.

## 테스트 기준
- 정상 응답과 오류 응답이 분리된다.
- JSON 필드가 일관된다.
