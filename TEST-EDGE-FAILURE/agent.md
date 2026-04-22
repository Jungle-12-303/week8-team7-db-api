# TEST-EDGE-FAILURE Agent Brief

## 목적
- 서버 레벨 엣지 케이스와 실패 복원 시나리오를 검증한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 엣지 케이스 고려 원칙을 따른다.
- 루트 `agent.md`의 정확성/안정성 검증 규칙을 따른다.

## 담당 범위
- 빈 body
- 너무 긴 SQL
- 클라이언트 중간 종료
- queue overflow
- worker exhaustion
- shutdown 중 요청
- 엔진 오류의 HTTP 오류 전환 확인

## 범위 제외
- 일반 기능 테스트
- 병렬성 성능 비교

## 인터페이스
- 입력: 실패 유도 시나리오
- 출력: 안전한 오류 응답과 복원 여부

## 소유 경로
- `TEST-EDGE-FAILURE/cases/**`
- `TEST-EDGE-FAILURE/scripts/**`

## 의존성
- `SERVER-HTTP`
- `SERVER-CONCURRENCY`
- `SERVER-RUNTIME`

## 완료 조건
- 서버 엣지 케이스 목록이 문서화된다.
- 오류 전환 정책과 실패 복원 시나리오가 준비된다.

## 테스트 기준
- 잘못된 요청이 프로세스 crash 없이 처리된다.
- shutdown/고갈/끊김 상황에서 안전성이 유지된다.
