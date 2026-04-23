# 루트 통합 이슈

## 열린 이슈
- `.codex` 수동 기록 스크립트는 호스트 권한 PowerShell에서는 동작하지만, 현재 Codex 샌드박스 셸에서는 ACL/권한 모델 때문에 직접 append가 실패할 수 있다
- Windows 호스트 PowerShell에서 `http://127.0.0.1:8080` direct 접근 timeout 원인은 아직 분리되지 않았다

## 해결됨
- 2026-04-23 KST
  - 부분 추가된 `INSERT` overlap 시연 자산은 유지하지 않고, 기존 운영 방식대로 `TEST-CONCURRENCY/work.md`의 후속 작업 항목으로만 남기도록 정리함
  - 루트 기준 문서에서 공용 실행 파일 표기를 `compose.yaml` 기준으로 정렬하고, curl 예시를 raw SQL body에 맞는 `--data-binary` 형식으로 통일함
  - `[LOCK]` 고정 형식 로그와 `[RUNTIME]` 작업 로그가 실제 구현/검증 상태와 루트 문서 설명 사이에서 어긋나던 표현을 정리함
  - 동시성 시연이 서버 내부 로그에 과도하게 의존하던 설명을 `TEST-CONCURRENCY` timing/timeline 출력 기준으로 재정렬해 발표 경로를 명확히 함
  - `.codex` 폴더가 "자동 기록 기능"처럼 보이던 혼선을 줄이기 위해 수동 운영 규칙과 기록 스크립트를 추가함
- 2026-04-22 KST
  - `build/bin/db_server`가 비어 있어 실제 API 서버를 띄울 수 없던 상태를 루트 빌드/Docker 경로 추가로 해소함
  - `SERVER-HTTP` 구현과 테스트 트랙 간 요청/응답 계약 불일치를 정리함
  - 이전 프로젝트 기준 트랙명과 문서 우선순위 하드코딩을 제거함
  - 루트 `PLAN.md`를 공통 명세 원본으로 재정렬함
