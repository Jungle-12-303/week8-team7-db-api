# 루트 진행 현황

## 진행중
- 현재 진행중인 루트 작업 없음

## 업데이트 필요
- 새 작업 폴더를 추가로 만들면 공통 템플릿(`agent.md`, `context.md`, `work.md`, `error.md`, `.codex/`)을 함께 적용할 것
- 루트 소유 공용 파일(`Dockerfile`, `compose.yaml`, `.dockerignore`, `Makefile`)이 추가되면 루트 기준으로만 관리할 것
- 공통 로그 형식이나 발표 시연 경로가 바뀌면 루트 `README.md`, `PLAN.md`, `howtouse.md`를 함께 갱신할 것
- `.codex` 기록 규칙이나 보조 스크립트가 바뀌면 루트 `agent.md`, `context.md`, `.codex/README.md`를 함께 갱신할 것
- 미구현 테스트/데모 케이스는 자산을 부분 추가하지 말고 해당 테스트 폴더 `work.md`의 `업데이트 필요`로만 남길 것

## 완료
- 2026-04-23 KST
  - 미완성 `INSERT` overlap 시연 케이스는 테스트 자산으로 유지하지 않고 `TEST-CONCURRENCY/work.md`의 `업데이트 필요`로만 관리하도록 정리함
  - 루트 `agent.md`, `work.md`, `PLAN.md`, `error.md`를 현재 `[HTTP]`, `[RUNTIME]`, `[LOCK]` 구현 상태와 열린 이슈 기준으로 다시 최신화함
  - 루트 `README.md`를 발표용 문서로 정리하고 구조 연결도, 동시성 처리 이론, API 명세 흐름을 mermaid 다이어그램으로 추가함
  - 루트 `agent.md`, `PLAN.md`, `howtouse.md`를 `TEST-CONCURRENCY` timing/timeline 출력과 현재 시연 흐름 기준으로 재정렬함
  - `[HTTP]`, `[RUNTIME]`, `[LOCK]` 로그가 실제 구현 상태에 맞게 루트 계획 문서에 반영되도록 정리함
  - `.codex` 운영 규칙을 문서로 고정하고 `append-history.ps1`, `new-session-note.ps1` 수동 기록 스크립트를 추가함
  - `SERVER-CONCURRENCY`, `SERVER-RUNTIME` 하위 문서와 루트 기준 문서를 다시 대조해 `[LOCK]` 고정 형식 로그와 `[RUNTIME]` 작업 로그 구현 완료 상태를 반영함
- 2026-04-22 KST
  - 루트 `Makefile`, `Dockerfile`, `compose.yaml`을 추가해 `db_server` 빌드/실행 경로를 연결함
  - Docker 컨테이너에서 `make server` 빌드와 `/health`, `/query` 내부 스모크 응답을 확인함
  - 각 작업 폴더의 `work.md`, `error.md`를 현재 구현/검증 결과 기준으로 다시 정렬함
  - `TEST-ENGINE-ADAPTER`를 `SERVER-CORE` 공개 API 기준으로 전환하고 38개 계약 검증 재통과를 확인함
  - `TEST-HTTP-FUNCTIONAL`, `TEST-CONCURRENCY`, `TEST-EDGE-FAILURE` 테스트 계약을 현재 `SERVER-HTTP` 응답 스키마에 맞게 정리함
  - 루트 문서 체계를 `PLAN.md` 중심으로 재구성함
  - 루트 `agent.md`, `context.md`, `work.md`, `error.md` 역할을 정리함
  - `multi-agent-collaboration` 스킬의 프로젝트별 하드코딩 예시를 제거함
  - 루트 `agent.md`와 협업 스킬에 페르소나 기반 검증 규칙을 복원함
  - 루트 바로 아래에 `SERVER-*`, `TEST-*` 세분화 폴더 구조를 생성함
  - 각 작업 폴더의 빈 구현/테스트 디렉터리에 README 플레이스홀더를 추가함
