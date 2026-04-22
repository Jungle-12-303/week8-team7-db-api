# SERVER-CORE include

- `week8-team7-db-api` 엔진을 외부에서 호출하기 위한 공개 헤더를 둡니다.
- 예시: `engine_api.h`
- `engine_api.h`는 SQL 요청 구조체, 공용 실행 결과, 엔진 콜백 계약을 정의합니다.
- 출력 캡처 규칙:
  - 엔진이 `FILE *out`에 기록한 텍스트는 `output_text`로 반환됩니다.
  - 짧은 성공/실패 요약은 `message`로 반환됩니다.
