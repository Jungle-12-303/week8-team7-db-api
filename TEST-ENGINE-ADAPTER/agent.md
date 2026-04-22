# TEST-ENGINE-ADAPTER Agent Brief

## 목적
- `SERVER-CORE` 엔진 어댑터의 입력/출력과 오류 전달을 검증한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 엔진 재사용 기준을 따른다.
- 루트 `agent.md`의 테스트/회귀 검증 규칙을 따른다.

## 담당 범위
- SQL 문자열 -> 어댑터 결과 구조체 검증
- 출력 캡처 문자열 검증
- parse/execute 오류 전달 검증

## 범위 제외
- HTTP 프로토콜 테스트
- thread pool과 lock 정책 검증

## 인터페이스
- 입력: 엔진 어댑터 실행 함수
- 출력: 결과 구조체와 캡처된 출력 검증 결과

## 소유 경로
- `TEST-ENGINE-ADAPTER/cases/**`
- `TEST-ENGINE-ADAPTER/scripts/**`

## 의존성
- `SERVER-CORE`
- `week8-team7-db-api`

## 완료 조건
- 정상 SQL/오류 SQL 모두 어댑터 수준에서 검증 가능하다.

## 테스트 기준
- 결과 구조체 일관성
- 출력 캡처 일관성
- 엔진 회귀와 어댑터 연결 확인
