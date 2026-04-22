# HTTP Error Mapping

`TEST-EDGE-FAILURE`는 "서버가 crash 없이 안전한 오류 응답을 만들고, 이후에도 복구 가능한가"를 우선 검증합니다.

## 기본 전환 정책
| 상황 | 우선 상태 코드 | 허용 보조 코드 | 이유 | 복원 확인 |
| --- | --- | --- | --- | --- |
| 빈 body | `400 Bad Request` | 없음 | 요청 자체가 잘못됨 | 즉시 `/health` 200 |
| 너무 긴 SQL | `413 Payload Too Large` | 없음 | 서버 상한 초과 | 즉시 `/health` 200 |
| 잘못된 debug header | `400 Bad Request` | 없음 | 헤더 값이 계약을 위반함 | 즉시 `/health` 200 |
| 잘못된 SQL 문법/사용자 입력 오류 | `400 Bad Request` | 없음 | 엔진이 거부한 사용자 입력 | 즉시 `/health` 200 |
| 엔진 내부 오류 | `500 Internal Server Error` | 없음 | 서버 내부 실패 | 즉시 `/health` 200 |
| queue overflow | `503 Service Unavailable` | 없음 | 현재 런타임은 queue full을 `server_busy` + `503`으로 고정함 | burst 후 `/health` 200 |
| worker exhaustion | `503 Service Unavailable` | 없음 | 현재 런타임은 수용 불가를 `503` 계열로 정리함 | 부하 종료 후 `/health` 200 |
| shutdown 중 신규 요청 | `503 Service Unavailable` | 없음 | 현재 런타임은 queue closed를 `server_stopping` + `503`으로 고정함 | 재기동 후 `/health` 200 |
| 클라이언트 중간 종료 | 응답 보장 없음 | 연결 종료 | peer가 먼저 끊겼으므로 서버 생존성만 확인 | 이후 `/health` 200 |
| shutdown 중 진행 중 요청 | 응답 완료 또는 연결 종료 | 없음 | drain 타이밍과 peer 상태에 따라 결과가 갈릴 수 있음 | 재기동 후 `/health` 200 |

## 해석 원칙
- `4xx`는 클라이언트 입력 문제에만 사용합니다.
- `5xx`는 서버 내부 실패 또는 일시적 수용 불가에만 사용합니다.
- 현재 프로젝트 계약에서는 overload와 shutdown drain을 `500`이 아니라 `503`으로 고정합니다.
- in-flight 요청은 graceful drain 중에도 응답 완료와 연결 종료가 모두 가능하므로 타이밍 허용치를 둡니다.

## 현재 한계
- 엔진 내부 오류를 black-box HTTP만으로 안정적으로 유도하기 어렵기 때문에, 현재 자동 케이스는 우선 "잘못된 SQL -> 오류 응답"을 검증합니다.
- `500` 고정 검증은 향후 `SERVER-CORE` 또는 `SERVER-RUNTIME`에 fault injection 지점이 생기면 추가합니다.
