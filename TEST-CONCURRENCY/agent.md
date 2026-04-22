# TEST-CONCURRENCY Agent Brief

## 목적
- 병렬 SQL 요구와 락 정책 검증을 전담한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 병렬 SQL 검증 요구를 따른다.
- 루트 `agent.md`의 동시성/안정성 검증 규칙을 따른다.

## 담당 범위
- 동시 `SELECT`
- 동시 `INSERT`
- `SELECT + INSERT`
- wall-clock 기반 병렬성 검증

## 범위 제외
- HTTP 기능 전체 검증
- 엔진 어댑터 출력 포맷 검증

## 인터페이스
- 입력: 동시 요청 시나리오
- 출력: 병렬 처리 여부와 일관성 검증 결과

## 소유 경로
- `TEST-CONCURRENCY/cases/**`
- `TEST-CONCURRENCY/scripts/**`

## 의존성
- `SERVER-CONCURRENCY`
- `SERVER-RUNTIME`
- `SERVER-HTTP`

## 완료 조건
- sequential/concurrent 비교 기준이 문서화된다.
- 병렬 `SELECT`와 직렬 `INSERT` 정책 검증 시나리오가 준비된다.

## 테스트 기준
- concurrent `SELECT` 총 시간이 sequential보다 짧거나 병렬 처리 흔적이 있어야 한다.
- 혼합 요청에서 데이터 일관성이 유지되어야 한다.
