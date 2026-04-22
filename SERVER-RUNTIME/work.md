# SERVER-RUNTIME 진행 현황

## 진행중
- 현재 진행중인 로컬 작업 없음

## 업데이트 필요
- graceful shutdown 제어면 확장
- 실제 기능/동시성 테스트 러너와 live server 경로 연결

## 완료
- 2026-04-22 KST
  - `db_server_runtime.c`, `db_server_main.c`를 추가해 listener, accept loop, worker submit, signal 기반 종료 경로를 구현함
  - 루트 `Makefile`과 Docker 경로를 통해 `build/bin/db_server` 빌드 및 실행 경로를 연결함
  - Docker 컨테이너 내부에서 `/health`, `/query` 스모크 응답을 확인함
  - SERVER-RUNTIME 템플릿 폴더와 기록 구조를 생성함
  - `server_runtime/runtime_config.h`, `server_runtime/server_main.h` 공개 인터페이스를 추가함
  - CLI/env 기반 포트, worker, repo root, schema/data 경로 로더를 구현함
  - `run-server.sh`, `smoke-health.sh` 런타임 보조 스크립트를 추가함
