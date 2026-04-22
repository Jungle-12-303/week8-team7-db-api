# insert_ok

## 목적
- 성공한 `INSERT`가 표 출력 없이 결과 구조체만 일관되게 반환되는지 확인한다.

## SQL
```sql
INSERT INTO users (name, age) VALUES ('Alice', 20);
```

## Fixture
- schema: `table=users`, `columns=name,age`
- data rows: header only

## Expected Result
- `ok = 1`
- `affected_rows = 1`
- `message = "INSERT 1"`
- `output_text = ""`
- data file contains `Alice,20`
