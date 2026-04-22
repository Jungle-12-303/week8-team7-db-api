# TEST-CONCURRENCY Agent Brief

## 목적
- 동시 SQL 요청 수용, `serial_all` 기본 락 정책, API 계층 overlap 검증을 전담한다.

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
- `X-Debug-Sleep-Ms` 기반 API 계층 overlap 시연 케이스

## 범위 제외
- HTTP 기능 전체 검증
- 엔진 어댑터 출력 포맷 검증
- trace/log 출력 기능 자체 구현

## 인터페이스
- 입력: raw SQL body 기반 동시 요청 시나리오, 선택적 `X-Debug-Sleep-Ms` 헤더 시나리오
- 출력: wall-clock 비교, `max_parallel`, `start_spread_ms`, `overlap_window_ms`, `concurrency_confirmed`, 요청별 START/END timeline, 응답 일관성, `serial_all` 정책 관측 결과

## 소유 경로
- `TEST-CONCURRENCY/cases/**`
- `TEST-CONCURRENCY/scripts/**`

## 의존성
- `SERVER-CONCURRENCY`
- `SERVER-RUNTIME`
- `SERVER-HTTP`

## 완료 조건
- sequential/concurrent 비교 기준이 문서화된다.
- `serial_all` 기본 정책 기준 실제 서버 관측 결과의 참조 경로가 정리된다.
- 병렬 `SELECT`, 직렬 `INSERT`, 혼합 요청, debug sleep overlap 검증 시나리오가 준비된다.
- 서버 `stdout` 없이도 러너 콘솔 출력만으로 overlap과 시작 시점 밀집도를 설명할 수 있다.

## 테스트 기준
- self-test 또는 `READERS_PARALLEL` 경로에서는 concurrent `SELECT` 총 시간이 sequential보다 짧거나 병렬 처리 흔적이 있어야 한다.
- 현재 runtime 기본값 `CONCURRENCY_LOCK_POLICY_SERIAL_ALL`에서는 concurrent `INSERT`가 sequential baseline보다 과도하게 빨라지지 않고 후행 조회 일관성이 유지되어야 한다.
- `X-Debug-Sleep-Ms` 케이스에서는 API 계층에서 `max_parallel > 1`, bounded `start_spread_ms`, positive `overlap_window_ms` 또는 sequential 대비 짧은 총 시간으로 overlap 흔적이 보여야 한다.
- 혼합 요청에서 데이터 일관성이 유지되어야 한다.
