# Classic SQL constructs not supported yet

## \[NOT\] \[EXISTS|INTERSECT\|EXCEPT] {#not-exists}

A syntactically available alternative is `EXISTS`, but it's not very useful as it doesn't support correlated subqueries. You can also rewrite it using `JOIN`.

## UNION {#union}

An available alternative is `SELECT DISTINCT` by explicitly listed columns + `UNION ALL` with subqueries (if needed).

## NATURAL JOIN {#natural-join}

An alternative is to explicitly list the matching columns on both sides.

## NOW() / CURRENT_TIME() {#now}

An alternative is to use the functions `CurrentUtcDate`, `CurrentUtcDatetime` and `CurrentUtcTimestamp`.

