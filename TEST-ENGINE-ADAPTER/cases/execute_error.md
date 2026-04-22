# execute_error

## 목적
- executor 단계 오류가 어댑터 결과 구조체에 반영되고 출력 캡처는 비어 있는지 확인한다.

## SQL
```sql
SELECT missing FROM users;
```

## Fixture
- schema: `table=users`, `columns=name,age`
- data rows: `Alice,20`

## Expected Result
- `ok = 0`
- `affected_rows = 0`
- `message` contains `unknown column in SELECT: missing`
- `output_text = ""`
