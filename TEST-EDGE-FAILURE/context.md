# TEST-EDGE-FAILURE 컨텍스트

## 현재 상태
- 서버 실패와 엣지 케이스 검증 전용 폴더다.
- `cases/*.json`으로 실패 유도 시나리오를 기계적으로 정의했다.
- `scripts/run_edge_failure_cases.py`로 외부 서버 또는 managed subprocess에 대한 블랙박스 검증이 가능하다.
- `scripts/mock_edge_server.py`로 하네스 자체를 로컬에서 검증할 수 있다.
- empty body / oversized SQL / invalid SQL 케이스는 현재 HTTP 계층 기준 최종 상태 코드(`400`/`413`/`400`)로 좁혀 두었다.
- `SERVER-RUNTIME`의 signal 기반 shutdown 경로와 `SERVER-HTTP`의 `invalid_header` 계약을 케이스에 반영했다.
- overload/worker exhaustion 기대 상태 코드를 현재 런타임 계약에 맞춰 `200/503`으로 좁혔다.
- mock 서버 기준 8개 케이스 전체 PASS를 확인했다.

## 다음 작업
- Linux/Docker live server 기준으로 overload/worker exhaustion/shutdown 결과를 누적한다.
- 요청 추적 로그와 종료 요약 로그가 추가되면 invalid header, oversize body, overload 실패 응답의 로그 일관성을 검증한다.
- `500` fault injection 지점이 생기면 엔진 내부 오류 케이스를 추가한다.
