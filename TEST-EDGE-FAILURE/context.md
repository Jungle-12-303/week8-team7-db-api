# TEST-EDGE-FAILURE 컨텍스트

## 현재 상태
- 서버 실패와 엣지 케이스 검증 전용 폴더다.
- `cases/*.json`으로 실패 유도 시나리오를 기계적으로 정의했다.
- `scripts/run_edge_failure_cases.py`로 외부 서버 또는 managed subprocess에 대한 블랙박스 검증이 가능하다.
- `scripts/mock_edge_server.py`로 하네스 자체를 로컬에서 검증할 수 있다.
- empty body / oversized SQL / invalid SQL 케이스는 현재 HTTP 계층 기준 최종 상태 코드(`400`/`413`/`400`)로 좁혀 두었다.

## 다음 작업
- graceful shutdown 제어면이나 fault injection 지점이 생기면 shutdown/500 검증을 더 엄격하게 만든다.
