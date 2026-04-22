# SERVER-CORE Agent Brief

## 목적
- `week8-team7-db-api` 엔진을 외부 서버가 호출할 수 있는 공용 어댑터를 정의한다.

## 문서 우선순위
1. 루트 `PLAN.md`
2. 루트 `agent.md`
3. 현재 폴더 `agent.md`
4. `skills/multi-agent-collaboration/SKILL.md`
5. 현재 폴더 `work.md`, `error.md`, `context.md`, `.codex/`

## 공통 문서 및 스킬 참조
- 루트 `PLAN.md`의 엔진 재사용 및 API 서버 연결 원칙을 따른다.
- 루트 `agent.md`의 기록 규칙과 페르소나 기반 검증을 따른다.
- 협업 절차는 `skills/multi-agent-collaboration/SKILL.md`를 따른다.

## 담당 범위
- SQL 문자열 입력을 엔진 실행으로 연결하는 어댑터
- 엔진 실행 결과 구조체 정의
- `FILE *out` 기반 출력 캡처 규칙 정리

## 범위 제외
- HTTP 요청 파싱
- thread pool, queue, lock manager
- 서버 프로세스 기동

## 인터페이스
- 입력: SQL 문자열, schema/data 경로
- 출력: `ok`, `affected_rows`, `message`, `output_text` 형태의 공용 실행 결과

## 소유 경로
- `SERVER-CORE/include/**`
- `SERVER-CORE/src/**`

## 의존성
- `week8-team7-db-api`
- 루트 `PLAN.md`

## 완료 조건
- 외부 서버가 직접 엔진 내부 파일을 모르고도 호출 가능한 공개 인터페이스가 정의된다.
- 출력 캡처와 오류 전달 방식이 문서화된다.

## 테스트 기준
- 정상 SQL/오류 SQL 결과 구조체가 일관된다.
- 출력 문자열 캡처 규칙이 문서로 설명 가능하다.
