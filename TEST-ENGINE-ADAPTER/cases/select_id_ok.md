# select_id_ok

## 목적
- `WHERE id = ...` 인덱스 경로에서도 어댑터가 동일한 결과 구조를 유지하는지 확인한다.

## SQL
```sql
SELECT name FROM users WHERE id = 2;
```

## Fixture
- schema: `table=users`, `columns=name,age`
- data rows: `Alice,20`, `Bob,21`, `Carol,22`

## Expected Result
- `ok = 1`
- `affected_rows = 1`
- `message = "SELECT 1"`
- `output_text` contains `| name |`
- `output_text` contains `Bob`
- `output_text` does not contain `Alice`
