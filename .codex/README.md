# .codex 운영 규칙

이 폴더는 프로젝트 로컬 기록 저장소다.

구성:
- `history.jsonl`
  - 중요한 이벤트를 한 줄 JSON으로 append하는 로그
- `sessions/`
  - 필요할 때만 만드는 세션별 상세 메모 저장소
- `scripts/append-history.ps1`
  - `history.jsonl`에 한 줄 이벤트를 추가하는 수동 기록 스크립트
- `scripts/new-session-note.ps1`
  - `sessions/`에 세션 메모 파일을 만드는 수동 생성 스크립트

운영 원칙:
- 최신 상태 요약과 핸드오프는 루트 `context.md`가 담당한다.
- `.codex`는 별도 자동화가 없는 한 수동 운영 로그 구조로 본다.
- 즉, 폴더가 있다고 해서 자동 세션 기록 기능이 항상 동작한다고 가정하지 않는다.

사용 예시:

```powershell
powershell -ExecutionPolicy Bypass -File .codex\scripts\append-history.ps1 -Summary "Updated README for presentation" -Scope root -Actor codex -Source manual
```

```powershell
powershell -ExecutionPolicy Bypass -File .codex\scripts\new-session-note.ps1 -Topic "presentation-prep"
```
