# 루트 작업 운영 규칙

## 목적
- 루트 `PLAN.md`를 공통 요구사항, 아키텍처, API 명세, 검증 결과의 기준 문서로 유지한다.
- 루트 `agent.md`는 루트 문서 운영 규칙, 소유 범위, 검증 기준, 마무리 기준을 정의한다.
- 루트는 세부 기능 구현보다 공통 계약 관리, Docker 실행 경로 관리, 테스트 결과 취합, 최종 문서 정리에 집중한다.

## 루트 소유 범위
- 공통 문서
  - `PLAN.md`
  - `agent.md`
  - `context.md`
  - `work.md`
  - `error.md`
  - `README.md`
  - `howtouse.md`
  - `commit-rules.md`
- 루트 기록 문서
  - `.codex/README.md`
  - `.codex/history.jsonl`
  - `.codex/sessions/README.md`
- 루트 공용 실행 파일
  - `Dockerfile`
  - `compose.yaml`
  - `.dockerignore`
  - `Makefile`
- 루트 바로 아래 작업 디렉터리
  - `week8-team7-db-api`
  - `SERVER-CORE`
  - `SERVER-HTTP`
  - `SERVER-CONCURRENCY`
  - `SERVER-RUNTIME`
  - `TEST-ENGINE-ADAPTER`
  - `TEST-HTTP-FUNCTIONAL`
  - `TEST-CONCURRENCY`
  - `TEST-EDGE-FAILURE`

## 문서 확인 순서
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 루트 `howtouse.md`
4. 루트 `context.md`
5. 루트 `work.md`
6. 루트 `error.md`
7. 현재 작업 디렉터리의 `agent.md`, `context.md`, `work.md`, `error.md`
8. 필요할 때만 현재 작업 디렉터리 `.codex/`

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 루트 `howtouse.md`
4. 현재 작업 디렉터리 `agent.md`
5. 현재 작업 디렉터리 `context.md`, `work.md`, `error.md`
6. 현재 작업 디렉터리 `.codex/`

- 하위 문서가 루트 계약과 충돌하면 루트 문서를 우선한다.
- 공통 API, Docker 실행 방식, 테스트 결과, 로그 계약이 바뀌면 먼저 루트 `PLAN.md` 또는 루트 `agent.md`를 갱신한다.
- 발표/데모 설명 문서가 바뀌면 루트 `README.md`와 `howtouse.md`를 함께 점검한다.

## 루트 역할
- 공통 API 계약 관리
- 공통 아키텍처와 디렉터리 책임 경계 관리
- Docker/빌드/실행 경로 관리
- 사용자 실행 절차와 데모 절차 문서 관리
- live 검증 결과와 로그 시연 절차 취합
- 발표용 설명 자료와 다이어그램 관리
- `TEST-CONCURRENCY` 같은 테스트 러너 출력 형식이 시연 요구를 충족하는지 우선 점검
- 하위 디렉터리 진행 현황 취합
- 테스트 계약 정렬 상태 점검
- 최종 마무리와 루트 문서 최신화

## 하위 디렉터리 역할
- 자기 범위 코드 구현과 로컬 테스트
- 로컬 `work.md`, `error.md`, `context.md` 업데이트
- 루트 계약을 기준으로 세부 문서 보강

## 변경 규칙
- 루트 소유 파일은 공통 계약, 실행 경로, 검증 결과, 문서 체계가 바뀌는 경우에만 수정한다.
- 다른 디렉터리 소유 파일은 해당 범위 작업이 아니면 건드리지 않는다.
- 하위 구현이 루트 계약과 어긋나면 먼저 루트 문서와 실제 코드 중 어느 쪽을 기준으로 정리할지 판단하고, 결정이 끝난 뒤 양쪽을 맞춘다.
- 사용자가 "테스트 출력만으로 동시성이 보여야 한다"는 요구를 주면 `SERVER-*` 로그 확장보다 `TEST-CONCURRENCY` 출력 형식 보강을 우선한다.
- 시연/테스트 아이디어가 아직 구현되지 않았다면 케이스 파일을 부분 추가하지 말고, 해당 `TEST-*` 폴더 `work.md`의 `업데이트 필요`에만 남긴다.

## 기록 운영 규칙
- `context.md`
  - 최신 상태 요약과 핸드오프만 남긴다.
  - 긴 작업 로그, 명령 전체 출력, 대화 원문은 넣지 않는다.
  - 구조는 `현재 목표`, `확정된 결정`, `최근 작업 메모`, `다음 핸드오프` 중심으로 유지한다.
  - 큰 방향, 공통 계약, 다음 작업자가 바로 알아야 할 맥락이 바뀌면 갱신한다.
- `.codex/history.jsonl`
  - 프로젝트 로컬 이벤트 로그다.
  - 한 줄당 하나의 JSON 객체로 append한다.
  - 남길 값은 `timestamp`, `scope`, `actor`, `summary`, `source`를 기본으로 본다.
  - 문서 체계 변경, 검증 완료, 중요한 결정처럼 나중에 추적 가치가 있는 이벤트만 남긴다.
- `.codex/sessions/`
  - 세션별 상세 메모나 원본 핸드오프를 두는 선택적 저장소다.
  - 필요할 때만 파일을 만들고, 자동 생성된다고 가정하지 않는다.
  - 파일명은 `YYYY-MM-DD-HHMM-topic.md`처럼 시각과 주제를 함께 드러내는 형식을 권장한다.
  - 길게 남길 필요가 없는 경우에는 `context.md`와 `history.jsonl`만 갱신하고 세션 파일은 생략해도 된다.
- `.codex` 전반
  - 별도 자동화가 없으면 수동 운영 문서/로그로 본다.
  - 즉, 폴더가 있다고 해서 자동 세션 기록 기능이 항상 작동한다고 가정하지 않는다.

## 검증 기준
- API 계약 또는 HTTP 테스트 변경 시:
  - `powershell -ExecutionPolicy Bypass -File TEST-HTTP-FUNCTIONAL\scripts\run-http-functional-tests.ps1 -ValidateOnly`
  - live 결과가 필요하면 `powershell -ExecutionPolicy Bypass -File TEST-HTTP-FUNCTIONAL\scripts\run-http-functional-tests.ps1 -Transport docker-compose-exec -BaseUrl http://127.0.0.1:8080`
  - `/health`, `/query`, `X-Debug-Sleep-Ms`, 현재 JSON 응답 스키마 일치 여부 확인
- 서버 런타임/빌드 변경 시:
  - `docker compose up -d server`
  - `docker compose exec server curl --fail --silent --show-error http://127.0.0.1:8080/health`
  - `docker compose exec server curl --fail --silent --show-error -H "Content-Type: text/plain; charset=utf-8" --data-binary "SELECT name FROM student WHERE id = 1;" http://127.0.0.1:8080/query`
- 요청 추적 로그 변경 시:
  - `docker compose logs --no-color --tail 50 server`
  - `[HTTP]`, `[RUNTIME]`, `[LOCK]` 로그 형식과 `thread=...`, `requested/effective`, `debug_sleep_ms` 필드 유무 확인
- 엔진/어댑터 경계 변경 시:
  - `powershell -ExecutionPolicy Bypass -File TEST-ENGINE-ADAPTER\scripts\run_adapter_contract_tests.ps1`
- 동시성 케이스 또는 시연 경로 변경 시:
  - `python TEST-CONCURRENCY\scripts\run_concurrency_case.py --self-test`
  - 필요 시 `docker compose run --rm dev bash -lc "python3 TEST-CONCURRENCY/scripts/run_concurrency_case.py TEST-CONCURRENCY/cases/concurrent_select_student.json TEST-CONCURRENCY/cases/concurrent_insert_student.json TEST-CONCURRENCY/cases/mixed_select_insert_student.json TEST-CONCURRENCY/cases/debug_sleep_overlap_select.json --base-url http://server:8080 --timeout 15"`
  - 결과 출력에 `start_spread_ms`, `overlap_window_ms`, `concurrency_confirmed`, START/END timeline이 보이는지 확인
- 경계 실패 케이스 변경 시:
  - mock 또는 live 기준 `TEST-EDGE-FAILURE` 결과와 현재 허용 상태 코드가 일치하는지 확인

## 이슈 기록 규칙
- 공통 이슈는 루트 `error.md`에 기록한다.
- 특정 디렉터리 구현/테스트 이슈는 해당 디렉터리 `error.md`에 기록한다.
- 후속 작업이 필요한 사항은 해당 범위 `work.md`에 남긴다.

## 완료 기준
- 루트 문서와 실제 코드/계약이 어긋나지 않는다.
- 변경한 범위에 맞는 검증이 끝났다.
- 남은 제한사항과 후속 작업이 문서에 반영됐다.
- 사용자가 바로 따라 할 수 있는 실행/시연 절차가 `howtouse.md`에 반영됐다.
- 발표/데모용 핵심 설명이 `README.md`에 반영됐다.
- 요청 추적 로그나 디버그 헤더 같은 데모 기능이 바뀌면 루트 `PLAN.md`, `howtouse.md`, 관련 `TEST-*` 문서가 같이 갱신됐다.
- 동시성 시연 요구가 테스트 러너 출력으로 충족되는 경우, 그 형식과 검증 기준이 루트 `PLAN.md`에 반영됐다.
- `.codex` 운영 방식이 바뀌면 루트 `agent.md`, `context.md`, `.codex/README.md`가 같이 갱신됐다.
- 다음 작업자가 바로 이어받을 수 있을 정도로 루트 기준이 최신 상태다.
