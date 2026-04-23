# 프로젝트 컨텍스트 요약

## 현재 목표
- 기존 SQL 처리기와 B+Tree 인덱스를 유지하면서 C 기반 HTTP API 서버를 추가한다.
- 서버는 thread pool과 job queue를 사용하되, 엔진 실행 구간은 전역 mutex로 직렬화한다.
- `/health`, `/query` 엔드포인트와 병렬성/엣지 케이스 검증 구조를 갖춘다.
- 실행 환경은 Docker 기반 Linux로 통일한다.

## 확정된 결정
- 공통 요구사항, 공통 아키텍처, 공통 API 명세의 원본은 루트 `PLAN.md`다.
- 루트 `agent.md`는 작업 절차와 문서 운영 규칙을 담당한다.
- 루트는 컨트롤 타워 역할만 하고, 세부 구현은 필요 시 하위 폴더에서 분리한다.
- `Dockerfile`, `docker-compose.yml`, `.dockerignore`, `Makefile`은 루트 소유다.
- HTTP 프로토콜은 `/health`, `/query`를 최소 엔드포인트로 시작한다.
- `context.md`는 최신 상태 요약과 핸드오프만 남긴다.
- `.codex/history.jsonl`은 중요한 이벤트를 append하는 로컬 이벤트 로그로 본다.
- `.codex/sessions/`는 필요할 때만 쓰는 수동 세션 메모 저장소로 본다.
- `.codex`는 별도 자동화가 없는 한 자동 기록 기능이 아니라 수동 운영 로그 구조로 본다.

## 최근 작업 메모
- 2026-04-22 KST
  - 문서 체계를 `PLAN.md` 중심으로 재구성했다.
  - 루트 `agent.md`를 작업 운영 규칙 문서로 재정의했다.
  - 루트 `work.md`, `error.md`, `.codex/` 구조를 추가했다.
  - `multi-agent-collaboration` 스킬에서 이전 프로젝트 기준 트랙명과 폴더 예시를 제거하고 일반화했다.
  - 루트 `agent.md`와 협업 스킬에 페르소나 기반 검증 절차를 추가했다.
  - 루트 바로 아래에 `SERVER-CORE`, `SERVER-HTTP`, `SERVER-CONCURRENCY`, `SERVER-RUNTIME`, `TEST-ENGINE-ADAPTER`, `TEST-HTTP-FUNCTIONAL`, `TEST-CONCURRENCY`, `TEST-EDGE-FAILURE` 폴더를 생성했다.
  - 각 작업 폴더에 공통 문서 템플릿과 `.codex/` 기록 구조를 생성했다.
  - 비어 있는 `include`, `src`, `cases`, `scripts` 디렉터리에 README 플레이스홀더를 추가해 이후 구현 위치를 명확히 했다.

## 다음 핸드오프
- 구현은 루트 바로 아래의 `SERVER-*`, `TEST-*` 폴더에서 시작한다.
- 각 폴더는 루트 `PLAN.md`를 복사하지 않고, 로컬 `agent.md`에서 자기 책임만 정의한다.
- 공통 계약 변경이 필요하면 로컬 문서보다 루트 `PLAN.md`를 먼저 갱신한다.
