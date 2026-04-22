# SERVER-CONCURRENCY Agent Brief

## 목적
- thread pool, job queue, lock manager를 통해 서버 동시성 정책을 담당한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 `read-read 병렬 / write-* 직렬` 정책을 따른다.
- 루트 `agent.md`의 동시성/안정성 검증 규칙을 따른다.

## 담당 범위
- worker thread 생성과 종료
- bounded job queue
- lock manager
- graceful shutdown 시 drain 정책

## 범위 제외
- HTTP request parsing
- 엔진 결과 직렬화
- 서버 프로세스 진입점

## 인터페이스
- 입력: 실행 작업 단위
- 출력: 실행 작업 완료/실패 통지
- `SELECT`와 `INSERT` 성격에 따라 lock 정책을 적용한다.

## 소유 경로
- `SERVER-CONCURRENCY/include/**`
- `SERVER-CONCURRENCY/src/**`

## 의존성
- `SERVER-CORE`
- `SERVER-HTTP`

## 완료 조건
- thread pool과 queue 책임이 문서화된다.
- `read-read 병렬 / write-* 직렬` 정책이 명시된다.

## 테스트 기준
- 병렬 `SELECT` 시 동시 실행 근거가 있어야 한다.
- `INSERT`와 혼합 요청에서 일관성 보장 정책이 설명 가능해야 한다.
