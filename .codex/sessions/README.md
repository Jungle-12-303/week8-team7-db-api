# Sessions

이 폴더는 프로젝트 로컬 세션 메모를 저장한다.

규칙:
- 요약과 핸드오프는 루트 `context.md`에 남긴다.
- 중요한 이벤트 한 줄 기록은 `.codex/history.jsonl`에 남긴다.
- 이 폴더는 필요할 때만 사용하며, 자동으로 채워진다고 가정하지 않는다.
- 세션 파일은 `YYYY-MM-DD-HHMM-topic.md` 형식을 권장한다.
- 긴 조사 메모, 발표 준비 초안, 임시 핸드오프처럼 `context.md`에 다 넣기엔 긴 내용을 둘 때 사용한다.

생성 예시:

```powershell
powershell -ExecutionPolicy Bypass -File .codex\scripts\new-session-note.ps1 -Topic "presentation-prep"
```
