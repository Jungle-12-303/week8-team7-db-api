# select_general_where_ok

## 목적
- 일반 `WHERE` 경로에서 어댑터가 결과 구조체와 출력 캡처 문자열을 함께 돌려주는지 확인한다.

## SQL
```sql
SELECT name FROM users WHERE age = 20;
```

## Fixture
- schema: `table=users`, `columns=name,age`
- data rows: `Alice,20`, `Bob,21`, `Carol,20`

## Expected Result
- `ok = 1`
- `affected_rows = 2`
- `message = "SELECT 2"`
- `output_text` contains `| name  |`
- `output_text` contains `Alice`
- `output_text` contains `Carol`
- `output_text` does not contain `Bob`
