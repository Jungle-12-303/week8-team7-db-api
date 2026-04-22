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
- `shutdown-during-request`는 `--server-command`가 있어야 자동으로 수행됩니다.
- mock 전용 헤더 `X-Mock-Sleep-Ms`는 실제 서버가 무시해도 무방합니다.
