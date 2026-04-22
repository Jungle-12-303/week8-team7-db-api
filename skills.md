# Skills Guide

이 저장소의 공통 작업 규칙과 스킬 참조 우선순위는 아래와 같다.

## 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. `work.md`, `error.md`, `context.md`, `.codex/`

## 역할 구분
- 루트 `PLAN.md`: 공통 요구사항, 공통 아키텍처, 공통 API 명세 원본
- 루트 `agent.md`: 작업 절차, 문서 운영 규칙, 에스컬레이션 규칙
- 현재 폴더 `agent.md`: 현재 폴더의 로컬 책임과 범위
- `skills/multi-agent-collaboration/SKILL.md`: 폴더 분리 작업을 위한 공통 협업 스킬 원본

## 사용 원칙
- 하위 폴더는 루트 `PLAN.md`를 복사하지 않는다.
- 하위 폴더는 루트 `PLAN.md`를 먼저 읽고, 현재 폴더 `agent.md`에서 로컬 범위를 확인한다.
- 스킬은 특정 프로젝트 이름이나 특정 폴더 경로를 하드코딩하지 않고, 루트 문서와 현재 폴더 기준으로 동작해야 한다.
