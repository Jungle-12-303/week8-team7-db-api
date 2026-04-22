# TEST-CONCURRENCY 진행 현황

## 진행중
- 현재 진행중인 로컬 작업 없음

## 업데이트 필요
- 현재 기록된 로컬 후속 작업 없음

## 완료
- 2026-04-22 KST
  - live `debug_sleep_overlap_select` 재검증에서 `start_spread_at_most` assertion 포함 PASS를 확인함
  - `run_concurrency_case.py` 출력에 phase별 `start_spread_ms`, `overlap_window_ms`, `concurrency_confirmed`, 요청별 상대 시각, START/END timeline을 추가함
  - `debug_sleep_overlap_select.json`에 `start_spread_at_most` assertion을 추가해 "거의 동시에 START" 기준을 결과에 직접 드러내도록 보강함
  - 서버 `stdout` 없이도 러너 콘솔 출력만으로 overlap을 설명할 수 있게 phase별 고정 형식 timing/timeline 출력을 정리함
  - `docker compose run --rm dev ... run_concurrency_case.py` 경로로 실제 서버 against `concurrent_select`, `concurrent_insert`, `mixed_select_insert`, `debug_sleep_overlap_select` 4케이스 재실행 PASS를 확인함
  - 루트 `Makefile`, `compose.yaml`, 루트 `agent.md`/`PLAN.md` 기준으로 `TEST-CONCURRENCY/scripts/run_concurrency_case.py` 통합 실행 경로가 연결된 상태를 확인함
  - 루트 `PLAN.md` 기준으로 `serial_all` 기본 정책의 실제 서버 end-to-end 결과와 `TEST-CONCURRENCY` 기본 3케이스 통과 기록이 정리된 상태를 확인함
  - `debug_sleep_overlap_select.json` 케이스와 mock `X-Debug-Sleep-Ms` 지원을 추가해 API 계층 overlap과 `max_parallel > 1` 시연 경로를 보강함
  - current `SERVER-HTTP` 계약에 맞춰 `output_text`, `message`, `affected_rows` 기준으로 케이스 기대값을 조정함
  - raw SQL text body 기준으로 mock 서버와 self-test 경로를 정리하고 재통과를 확인함
  - TEST-CONCURRENCY 템플릿 폴더와 기록 구조를 생성함
  - 병렬 `SELECT`, 직렬 `INSERT`, 혼합 `SELECT + INSERT` 케이스 JSON을 추가함
  - wall-clock 비교와 응답 일관성 검증을 수행하는 `scripts/run_concurrency_case.py`를 추가함
  - 내장 mock 서버 기반 `--self-test` 경로를 추가함
