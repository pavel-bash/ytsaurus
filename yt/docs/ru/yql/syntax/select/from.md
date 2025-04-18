# FROM

Источник данных для `SELECT`. В качестве аргумента может принимать имя строковой или колоночной таблицы, результат другого `SELECT` или [именованное выражение](../expressions.md#named-nodes). К именованным выражениям можно обращаться [как к таблицам](from_as_table.md)(`FROM AS_TABLE`).

Ещё в YQL можно выполнить запрос по нескольким таблицам. Для этого в `SELECT` после `FROM` можно указывать не только одну таблицу или подзапрос, но и вызывать встроенные функции, позволяющие объединять данные [нескольких таблиц](concat.md).

Также вы можете обходить не таблицы, а [итерироваться](walk_folders.md) по дереву целевого кластера, с возможностью накопить состояние, обычно список путей к таблицам.

Между `SELECT` и `FROM` через запятую указываются имена столбцов из источника или `*` для выбора всех столбцов.

Таблица по имени ищется в базе данных, заданной оператором [USE](../use.md).

## Примеры

```yql
SELECT key FROM my_table;
```

```yql
SELECT * FROM
  (SELECT value FROM my_table);
```

```yql
$table_name = "my_table";
SELECT * FROM $table_name;
```
