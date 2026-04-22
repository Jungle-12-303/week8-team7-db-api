# parse_error

## 목적
- parser 단계 오류가 어댑터 레벨에서 그대로 전달되는지 확인한다.

## SQL
```sql
SELECT name users;
```

## Expected Result
- `ok = 0`
- `affected_rows = 0`
- `message` contains `expected keyword FROM`
- `output_text = ""`
