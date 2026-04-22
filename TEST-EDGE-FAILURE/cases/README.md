# TEST-EDGE-FAILURE cases

이 폴더는 서버 레벨 엣지 케이스와 실패 복원 시나리오를 기계적으로 정의합니다.
`scripts/run_edge_failure_cases.py`는 아래 JSON 파일을 읽어 블랙박스 방식으로 검증합니다.

## 케이스 목록
- `01-empty-body.json`
  - `POST /query`에 빈 body를 보내도 4xx로 정리되고 `/health`가 유지되어야 합니다.
- `02-oversized-sql.json`
  - SQL 크기 상한을 넘는 요청이 crash 없이 거절되는지 확인합니다.
- `03-client-disconnect.json`
  - `Content-Length`를 선언한 뒤 일부만 보내고 연결을 끊어도 서버가 살아 있는지 확인합니다.
- `04-queue-overflow.json`
  - `worker_count + queue_capacity + burst_extra` 만큼 burst를 보내 queue overflow가 안전하게 거절되는지 확인합니다.
- `05-worker-exhaustion.json`
  - worker가 모두 바쁜 동안 추가 요청이 hang 없이 처리 또는 거절되는지 확인합니다.
- `06-shutdown-during-request.json`
  - signal 기반 graceful shutdown이 들어왔을 때 진행 중 요청과 종료 이후 복구를 확인합니다.
- `07-invalid-sql-error.json`
  - 잘못된 SQL이 2xx가 아닌 오류 응답으로 전환되는지 확인합니다.
- `08-invalid-debug-header.json`
  - 잘못된 `X-Debug-Sleep-Ms` 값이 `400 invalid_header`로 정리되는지 확인합니다.

## 상태 코드 기준
- 자세한 HTTP 오류 전환 정책은 [error-mapping.md](error-mapping.md)에서 관리합니다.
- 현재 `SERVER-HTTP` 구현이 이미 확정한 구간은 각 JSON의 허용 상태 코드를 최종 계약값으로 좁혀 두었습니다.
- 현재 `SERVER-RUNTIME` 구현을 기준으로 queue overflow와 shutdown 신규 요청은 `503`으로 고정합니다.
- in-flight 연결 종료처럼 peer/타이밍 영향이 남는 케이스만 보조 허용값을 유지합니다.

## 사용 메모
- 실제 `db_server` 런타임은 queue capacity를 `max(worker_count * 4, 8)`로 계산하므로 `--queue-capacity`도 그 값과 맞춰야 합니다.
- `shutdown-during-request`는 managed subprocess가 있어야 자동 수행되며, 가능하면 OS signal 기반 graceful stop을 사용하고 불가능하면 terminate로 fallback합니다.
