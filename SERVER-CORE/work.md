# SERVER-CORE 진행 현황

## 진행중
- 공개 엔진 어댑터 API 구현 완료
- week8 엔진 callback 기반 production 경로 연결 완료

## 업데이트 필요
- week8 엔진 callback을 공용 production source로 정리
- 필요 시 엔진 fault injection 훅 추가

## 완료
- 2026-04-22 KST
  - `week8_engine.c`, `week8_engine.h`를 추가해 production 런타임이 실제 엔진을 `SERVER-CORE` 경유로 호출하도록 연결함
  - `engine_api.h`, `engine_api.c` 공개 인터페이스와 `FILE *out` 캡처 어댑터를 구현함
  - `TEST-ENGINE-ADAPTER` 하네스를 `SERVER-CORE` 공개 API 기준으로 전환하고 38개 검증 재통과를 확인함
  - SERVER-CORE 템플릿 폴더와 기록 구조를 생성함
