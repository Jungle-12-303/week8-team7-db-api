# SERVER-CORE src

- SQL 문자열 입력, 엔진 호출, 출력 캡처를 담당하는 어댑터 구현 파일을 둡니다.
- 예시: `engine_api.c`
- `engine_api.c`는 엔진 콜백 실행, `FILE *out` 임시 스트림 캡처, 결과 문자열 정규화를 담당합니다.
- 어댑터 실패와 SQL 실패를 분리합니다.
  - 어댑터 실패: `server_core_status`
  - SQL/엔진 실패: `server_core_result.ok == 0`
