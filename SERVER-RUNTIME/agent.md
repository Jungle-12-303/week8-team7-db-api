# SERVER-RUNTIME Agent Brief

## 목적
- 실제 서버 프로세스 실행 환경, 종료 제어, 런타임 진입점을 담당한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 실행 환경, 포트, worker 연결 원칙을 따른다.
- 루트 `PLAN.md`의 `shutdown-during-request` 제한과 Docker 기준 실행 원칙을 따른다.
- 루트 `agent.md`의 운영/빌드 검증 규칙을 따른다.

## 담당 범위
- `server_main`, `db_server_main`
- 설정 로딩
- 포트/worker 개수 연결
- schema/data 경로 연결
- listener/accept loop와 HTTP/core/concurrency wiring
- graceful shutdown 진입점과 stop request 전달
- 런타임 trace/log/summary 출력 경계
- live server 실행 경로와 테스트 러너 연결
- 실행 스크립트

## 범위 제외
- HTTP 프로토콜 파싱
- thread pool 내부 구현
- 엔진 호출 어댑터 세부 구현

## 인터페이스
- 입력: 설정 값, 포트, worker 개수, 실행 경로, 종료 신호/stop 요청
- 출력: 실제 서버 프로세스 기동/종료, 런타임 로그, shutdown 요약
- HTTP method/path/status 같은 요청 메타데이터는 `SERVER-HTTP`가 노출한 계약을 통해서만 받는다.

## 소유 경로
- `SERVER-RUNTIME/include/**`
- `SERVER-RUNTIME/src/**`
- `SERVER-RUNTIME/scripts/**`

## 의존성
- `SERVER-CORE`
- `SERVER-HTTP`
- `SERVER-CONCURRENCY`
- `week8-team7-db-api`

## 완료 조건
- 서버 프로세스를 조립하는 진입점 책임이 정리된다.
- 실행 스크립트와 설정 연결 범위가 문서화된다.
- 종료 제어와 live 실행 경로가 문서화된다.
- trace/log 책임과 `SERVER-HTTP`, `SERVER-CONCURRENCY` 의존 경계가 정리된다.

## 테스트 기준
- 서버 시작/종료 흐름이 설명 가능해야 한다.
- 설정과 경로 연결 규칙이 문서로 정리되어야 한다.
- Docker 기준 live 실행 경로와 테스트 러너 연결 방법이 설명 가능해야 한다.
- trace/log 추가 전에는 요청 메타데이터 노출 계약이 어디서 필요한지 설명 가능해야 한다.
