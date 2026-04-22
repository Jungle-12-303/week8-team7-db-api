# SERVER-CORE 컨텍스트

## 현재 상태
- SQL 처리기 내부를 직접 수정하기보다 재사용용 엔진 어댑터를 정의하는 폴더다.
- `engine_api.h`, `engine_api.c` 공개 인터페이스와 `FILE *out` 기반 출력 캡처 계층이 구현됐다.

## 다음 작업
- week8 엔진 callback을 공용 source로 정리하고 런타임 조립 경로에 연결한다.
