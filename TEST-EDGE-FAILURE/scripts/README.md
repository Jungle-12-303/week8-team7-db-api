# TEST-EDGE-FAILURE scripts

장애/엣지 케이스 재현 및 복원 검증 스크립트를 둡니다.

## 파일
- `run_edge_failure_cases.py`
  - `cases/*.json`을 읽어 외부 서버 또는 managed subprocess에 대해 블랙박스 검증을 수행합니다.
- `mock_edge_server.py`
  - 실제 서버가 아직 없을 때 하네스를 검증하기 위한 로컬 mock 서버입니다.

## 예시
```powershell
python scripts/run_edge_failure_cases.py --host 127.0.0.1 --port 8080 --worker-count 4 --queue-capacity 8
```

```powershell
python scripts/run_edge_failure_cases.py `
  --host 127.0.0.1 `
  --port 18080 `
  --worker-count 2 `
  --queue-capacity 2 `
  --server-command "python scripts/mock_edge_server.py --port 18080 --workers 2 --queue-capacity 2 --sql-bytes-limit 2048"
```

## 주의
- 실제 `db_server` 런타임 기준 queue capacity는 `max(worker_count * 4, 8)`입니다.
- mock 서버 예시는 mock에 넘긴 `--queue-capacity`와 동일한 값을 하네스에도 넘겨야 합니다.
- `shutdown-during-request`는 `--server-command`가 있어야 자동으로 수행되며, 가능하면 graceful signal을 먼저 보내고 불가능하면 terminate로 fallback합니다.
- mock 전용 헤더 `X-Mock-Sleep-Ms`는 실제 서버가 무시해도 무방합니다.
