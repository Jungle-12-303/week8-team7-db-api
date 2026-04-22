# SERVER-HTTP 이슈

## 열린 이슈
- 현재 Windows 호스트에는 POSIX socket 헤더를 포함한 Linux toolchain이 없어 `http_server.c`의 실제 POSIX 컴파일 확인이 막혀 있다.
- WSL 환경에도 `gcc`가 없어 로컬 Linux shell 기준 검증은 못 했지만, Docker Linux 컨테이너 빌드 경로는 확보했다.

## 해결됨
- 2026-04-22 KST
  - 루트 Docker 빌드 경로를 추가해 Linux 컨테이너에서 `db_server` 빌드와 내부 스모크 응답 확인이 가능해짐
  - HTTP 기능 테스트 계약과 실제 구현 간 raw SQL body/JSON 필드 불일치를 정리함
  - HTTP 처리 전용 폴더 구조를 분리함
