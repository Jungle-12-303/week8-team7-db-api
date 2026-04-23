# SERVER-RUNTIME 진행 현황

## 진행중
- 현재 진행중인 로컬 작업 없음

## 업데이트 필요
- graceful shutdown 제어면 구체화
  `SIGINT/SIGTERM`, `request_stop`, 내부 `drain/cancel_pending` 전환 기준을 CLI/env 또는 문서 계약으로 노출할 것
- Docker 기준 live 검증 결과 누적
  Windows 호스트에서 `build/bin/db_server`는 직접 실행되지 않으므로 `docker compose` 기준으로 `TEST-HTTP-FUNCTIONAL`, `TEST-CONCURRENCY` 러너 결과를 다시 기록할 것
- 요청 단위 trace 계약 연동
  `req_id`, method, path, op, worker, status`는 현재 연동되었고, `lock_mode`, 요청/DB/응답 시각은 후속 로그 확장에서 보강할 것
- 실시간 요청 로그 포맷 고정
  `요청 수신`, `스레드 할당`, `작업 종료`, `락 대기/획득/해제`, `응답 완료`는 현재 고정 형식으로 보이고, `DB 작업 시작/종료`는 후속 추가
- 종료 요약 로그 추가
  shutdown 시점에 total/success/fail, op별 건수, 평균/최대 total_ms, db_ms 같은 요약 표를 출력할 것
- 로그 출력 직렬화 추가
  멀티스레드 환경에서도 줄이 섞이지 않도록 로그 전용 mutex 또는 동등한 직렬화 수단을 런타임에 둘 것

## 완료
- 2026-04-22 KST
  - `db_server_runtime.c`, `db_server_main.c`를 추가해 listener, accept loop, worker submit, signal 기반 종료 경로를 구현함
  - 루트 `Makefile`과 Docker 경로를 통해 `build/bin/db_server` 빌드 및 실행 경로를 연결함
  - Docker 컨테이너 내부에서 `/health`, `/query` 스모크 응답을 확인함
  - Windows 호스트에서 `build/bin/db_server`가 Linux 실행 파일이라 직접 기동되지 않음을 확인했고, 표준 live 실행 경로를 Docker Compose 기준으로 유지함
  - SERVER-RUNTIME 템플릿 폴더와 기록 구조를 생성함
  - `server_runtime/runtime_config.h`, `server_runtime/server_main.h` 공개 인터페이스를 추가함
  - CLI/env 기반 포트, worker, repo root, schema/data 경로 로더를 구현함
  - `run-server.sh`, `smoke-health.sh` 런타임 보조 스크립트를 추가함
  - per-request `http_request_trace`를 job context에 연결하고 `assigned`/`notify` 콜백으로 `[RUNTIME] | event=스레드 할당|작업 종료 | ...` 로그를 출력하도록 반영함
  - rejection 경로에서 이미 닫힌 client fd를 cleanup이 다시 닫지 않도록 double-close 위험을 제거함
  - Docker live runtime 기준 `TEST-CONCURRENCY` 4개 케이스를 재실행해 PASS와 로그 출력을 함께 확인함
