# CREATE MATERIALIZED VIEW

## Description

Creates a materialized view. Creating a materialized view is asynchronous operation. Running this command successfully indicates that the task of creating the materialized view is submitted successfully. You can view the building status of Sync Refresh synchronous materialized views in a database via [SHOW ALTER MATERIALIZED VIEW](../data-manipulation/SHOW%20ALTER%20MATERIALIZED%20VIEW.md) command. For usage information about materialized views, see [materialized view](../../../using_starrocks/Materialized_view.md).

> **CAUTION**
>
> Only users with the `CREATE_PRIV` privilege in the database where the base table resides can create a materialized view.

StarRocks supports asynchronous materialized views from v2.4. The major differences between asynchronous materialized views and synchronous materialized views in previous versions are as follows:

|                              | **ASYNC and MANUAL Refresh** | **Aggregated Column** | **Partitioning and Bucketing Changes** | **JOIN, WHERE, and GROUP BY clause** |
| ---------------------------- | ---------------------------- | ------------ | -------------------- | ------------------------------ |
| **Synchronous materialized view** | No (Materialized view in v2.3 and earlier only supports SYNC refresh)| No | No  | No |
| **Asynchronous materialized view** | Yes | Yes | Yes  | Yes  |

In StarRocks v2.5, asynchronous async refresh materialized views support query rewrite, nested materialized views, and creating materialized views based on Hive catalog, Hudi catalog, and Iceberg catalog.

## Syntax

```SQL
CREATE MATERIALIZED VIEW [IF NOT EXISTS] [database.]<mv_name>
-- distribution_desc
[DISTRIBUTED BY HASH(<bucket_key>) [BUCKETS <bucket_number>]]
-- refresh_desc
[REFRESH 
-- refresh_moment
    [IMMEDIATE | DEFERRED]
-- refresh_scheme
    [ASYNC | ASYNC (START <start_time>) EVERY INTERVAL <refresh_interval> | MANUAL]
]
-- partition_expression
[PARTITION BY 
    {<date_column> | date_trunc(fmt, <date_column>)}
]
-- order_by_expression
[ORDER BY (<sort_key>)]
[COMMENT ""]
[PROPERTIES ("key"="value", ...)]
AS 
<query_statement>
```

Parameters in brackets [] is optional.

## Parameters

**mv_name** (required)

The name of the materialized view. The naming requirements are as follows:

- The name must consist of letters (a-z or A-Z), numbers (0-9) or underscores (_), and it can only start with a letter.
- The length of the name cannot exceed 64 characters.

> **CAUTION**
>
> Multiple materialized views can be created on the same base table, but the names of the materialized views in the same database cannot be duplicated.

**distribution_desc** (**Required** when creating asynchronous materialized view)

The bucketing strategy of the materialized view, in the form of `DISTRIBUTED BY HASH (k1[,k2 ...]) [BUCKETS <bucket_number>]`.

**refresh_moment** (optional)

The refresh moment of the materialized view. Default value: `IMMEDIATE`. Valid values:

- `IMMEDIATE`: Refresh the materialized view immediately after it is created.
- `DEFERRED`: The materialized view is not refreshed after it is created. You can manually refresh the materialized view or schedule regular refresh tasks.

**refresh_scheme_desc** (optional)

The refresh strategy of the materialized view. This parameter supports the following values:

- `ASYNC`: Asynchronous refresh mode. For fixed-interval automatic refresh, you need to specify the refresh start time, refresh interval using the following units: `DAY`, `HOUR`, `MINUTE`, and `SECOND`. If you do not specify the interval, the materialized view refreshes each time the data in the base tables changes.
- `MANUAL`: Manual refresh mode.

If this parameter is not specified, the default value `MANUAL` is used.

**partition_expression** (optional)

The partitioning strategy of the materialized view. As for the current version of StarRocks, only one partition expression is supported when creating a materialized view. This parameter supports the following values:

- Column: column name used for partitioning. Expression `PARTITION BY dt` means to partition according to the `dt` column.
- date_trunc function: Function used to truncate time unit. `PARTITION BY date_trunc("MONTH", 'dt')` means that the `dt` column is truncated to month as unit for partitioning. The date_trunc function supports truncating time to units including `YEAR`, `MONTH`, `DAY`, `HOUR`, and `MINUTE`.

If this parameter is not specified, the materialized view adopts no partitioning strategy by default.

**order_by** (optional)

Specifies the sort key of materialized view. If you do not specify the sort key, StarRocks chooses some of the prefix columns from SELECT columns as the sort keys, for example, in `select a, b, c, d`, sort keys can be `a` and `b`. This parameter has been supported since StarRocks v3.0.

**COMMENT** (optional)

Comment on the materialized view.

**PROPERTIES** (optional)

Properties of the materialized view.

- `replication_num`: The number of materialized view replicas to create.
- `storage_medium`: Storage medium type. `HDD` and `SSD` are supported.
- `partition_ttl_number`: The number of most recent materialized view partitions to keep. After the number of partitions exceeds this value, expired partitions will be deleted. StarRocks will periodically check materialized view partitions according to the time interval specified in the FE configuration item `dynamic_partition_check_interval_seconds`, and automatically delete expired partitions. When the value is `-1`, all partitions of the materialized view will be preserved. Default: `-1`.
- `partition_refresh_number`: In a single refresh, the maximum number of partitions to refresh. If the number of partitions to be refreshed exceeds this value, StarRocks will split the refresh task and complete it in batches. Only when the previous batch of partitions is refreshed successfully, StarRocks will continue to refresh the next batch of partitions until all partitions are refreshed. If any of the partitions fails to be refreshed, no subsequent refresh tasks will be generated. When the value is `-1`, the refresh task will not be split. Default: `-1`.
- `excluded_trigger_tables`: If a base table of the materialized view is listed here, automatic refresh task will not be triggered when the data in the base table is changed. This parameter only applies to load-triggered refresh strategy, and is usually used together with the property `auto_refresh_partitions_limit`. Format: `[db_name.]table_name`. When the value is an empty string, any data change in all base tables triggers the refresh of the corresponding materialized view. Default is an empty string.
- `auto_refresh_partitions_limit`: The number of most recent materialized view partitions that need to be refreshed when a materialized view refresh is triggered. You can use this property to limit the refresh range and reduce the refresh cost. However, because not all the partitions are refreshed, the data in the materialized view may not be consistent with the base table. Default: `-1`. When the value is `-1`, all partitions will be refreshed. When the value is a positive integer N, StarRocks sorts the existing partitions in chronological order, and refreshes N partitions from the most recent partition. If the number of partitions is less than N, StarRocks refreshes all existing partitions. If there are dynamic partitions created in advance in your materialized view, StarRocks refreshes the pre-created partitions first, and then the existing partitions. Therefore, when setting this parameter, make sure that you have reserved margins for pre-created dynamic partitions.

**query_statement** (required)

The query statement to create the materialized view. Its result is the data in the materialized view. The syntax is as follows:

```SQL
SELECT select_expr[, select_expr ...]
[GROUP BY column_name[, column_name ...]]
[ORDER BY column_name[, column_name ...]]
```

- select_expr (required)

  All columns in the query statement, that is, all columns in the materialized view schema. This parameter supports the following values:

  - Single column or aggregated column: a statement in the form of `SELECT a, b, c FROM table_a` (applicable to creating a single table materialized view) or `SELECT table_a.a, table_a.b, table_b.d,` (applicable to creating an asynchronous materialized view in StarRocks 2.4 or above only), where `a`, `b`, `c`, and `d` are the column names of the base tables. If you do not specify column names for the materialized view in the statement, the column names in the materialized view are also `a`, `b`, `c`, and `d`.
  - Expression: an expression in the form of `SELECT a+1 AS x, b+2 AS y, c*c AS z FROM table_a`, where `a+1`, `b+2` and `c*c` are expressions that contain the column names of the base tables, and `x`, `y` and `z` are the new column names of the materialized view.

  > **CAUTION**
  >
  > This parameter must contain at least one single column, and all specified columns can only be specified once.

- GROUP BY (optional)

  The GROUP BY column of the query. If this parameter is not specified, the data will not be grouped by default.

- ORDER BY (optional)

  The ORDER BY column of the query.

  - Columns in the ORDER BY clause must be declared in the same order as the columns in `select_expr`.
  - If this parameter is not specified, the system will automatically supplement the ORDER BY column according to relevant rules. If the materialized view is the AGGREGATE type, all GROUP BY columns are automatically used as sort keys. If the materialized view is not the AGGREGATE type, the system automatically chooses sort keys based on the prefix columns.
  - If the query statement contains a GROUP BY clause, the ORDER BY columns must be identical to the GROUP BY columns.

### Supported data types

- Asynchronous materialized views created based on the StarRocks default catalog support the following data types:

  - DATE
  - DATETIME
  - CHAR
  - VARCHAR
  - BOOLEAN
  - TINYINT
  - SMALLINT
  - INT
  - BIGINT
  - LARGEINT
  - FLOAT
  - DOUBLE
  - DECIMAL
  - ARRAY
  - JSON
  - BITMAP
  - HLL
  - PERCENTILE

> **NOTE**
>
> BITMAP, HLL, and PERCENTILE have been supported since v2.4.5.

- Asynchronous materialized views created based on the StarRocks external catalogs support the following data types:

  - Hive Catalog

    - INT/INTEGER
    - BIGINT
    - TIMESTAMP
    - STRING
    - VARCHAR
    - CHAR
    - DOUBLE
    - FLOAT
    - DECIMAL
    - ARRAY

  - Hudi Catalog

    - BOOLEAN
    - INT
    - DATE
    - TimeMillis/TimeMicros
    - TimestampMillis/TimestampMicros
    - LONG
    - FLOAT
    - DOUBLE
    - STRING
    - ARRAY
    - DECIMAL

  - Iceberg Catalog

    - BOOLEAN
    - INT
    - LONG
    - FLOAT
    - DOUBLE
    - DECIMAL(P, S)
    - DATE
    - TIME
    - TIMESTAMP
    - STRING
    - UUID
    - FIXED(L)
    - BINARY
    - LIST

### Correspondence of aggregate functions

When a query is executed with a materialized view, the original query statement will be automatically rewritten and used to query the intermediate results stored in the materialized view. The following table shows the correspondence between the aggregate function in the original query and the aggregate function used to construct the materialized view. You can select the corresponding aggregate function to build a materialized view according to your business scenario.

| **aggregate function in the original query**           | **aggregate function of the materialized view** |
| ------------------------------------------------------ | ----------------------------------------------- |
| sum                                                    | sum                                             |
| min                                                    | min                                             |
| max                                                    | max                                             |
| count                                                  | count                                           |
| bitmap_union, bitmap_union_count, count(distinct)      | bitmap_union                                    |
| hll_raw_agg, hll_union_agg, ndv, approx_count_distinct | hll_union                                       |


## Relavant session variables

The following variables control the behaviour of materialized view:

- `analyze_mv`: Whether and how to analyze the materialized view after refresh. Valid values are an empty string (Do not analyze), `sample` (Sampled statistics collection), and `full` (Full statistics collection). Default is `sample`.
- `enable_materialized_view_rewrite`: Whether to enable the automatic rewrite for materialized view. Valid values are `true` (Default since v2.5) and `false`.

## Usage notes

- Prior to StarRocks 2.4, materialized views only support aggregate functions on a single column. Query statements in the form of `sum(a+b)` are not supported.

- Prior to StarRocks 2.4, clauses such as JOIN, WHERE, and GROUP BY are not supported in the materialized view creation statements.

- The current version of StarRocks does not support creating multiple materialized views at the same time. A new materialized view can only be created when the one before is completed.

- A materialized view supports only one aggregate function for each column of the base table. Query statements such as `select sum(a), min(a) from table` are not supported.

- When using ALTER TABLE DROP COLUMN to drop a specific column in a base table, you must ensure that all materialized views of the base table contain the dropped column, otherwise the drop operation will fail. Before you drop the column, you must first drop all materialized views that contain the column.

- Creating too many materialized views for a table will affect the data load efficiency. When data is being loaded to the base table, the data in materialized view and base table will be updated synchronously. If a base table contains `n` materialized views, the efficiency of loading data into the base table is about the same as the efficiency of loading data into `n` tables.

- About nested materialized views:

  - The refresh strategy for each materialized view only applies to the corresponding materialized view.
  - Currently, StarRocks does not limit the number of nesting levels. In a production environment, we recommend that the number of nesting layers not exceed THREE.

- About external catalog materialized views:

  - External catalog materialized view only support async fixed-interval refresh and manual refresh.
  - Strict consistency is not guaranteed between the materialized view and the base tables in the external catalog.
  - Currently, building materialized views based on external resources is not supported.
  - Currently, StarRocks cannot perceive if the base table data in the external catalog has changed, so all partitions will be refreshed by default every time the base table is refreshed. You can manually refresh only some of partitions using [REFRESH MATERIALIZED VIEW](../data-manipulation/REFRESH%20MATERIALIZED%20VIEW.md).

## Example

The following examples are based on the base tables below:

```SQL
CREATE TABLE `lineorder` (
  `lo_orderkey` int(11) NOT NULL COMMENT "",
  `lo_linenumber` int(11) NOT NULL COMMENT "",
  `lo_custkey` int(11) NOT NULL COMMENT "",
  `lo_partkey` int(11) NOT NULL COMMENT "",
  `lo_suppkey` int(11) NOT NULL COMMENT "",
  `lo_orderdate` int(11) NOT NULL COMMENT "",
  `lo_orderpriority` varchar(16) NOT NULL COMMENT "",
  `lo_shippriority` int(11) NOT NULL COMMENT "",
  `lo_quantity` int(11) NOT NULL COMMENT "",
  `lo_extendedprice` int(11) NOT NULL COMMENT "",
  `lo_ordtotalprice` int(11) NOT NULL COMMENT "",
  `lo_discount` int(11) NOT NULL COMMENT "",
  `lo_revenue` int(11) NOT NULL COMMENT "",
  `lo_supplycost` int(11) NOT NULL COMMENT "",
  `lo_tax` int(11) NOT NULL COMMENT "",
  `lo_commitdate` int(11) NOT NULL COMMENT "",
  `lo_shipmode` varchar(11) NOT NULL COMMENT ""
) ENGINE=OLAP
DUPLICATE KEY(`lo_orderkey`)
COMMENT "OLAP"
PARTITION BY RANGE(`lo_orderdate`)
(PARTITION p1 VALUES [("-2147483648"), ("19930101")),
PARTITION p2 VALUES [("19930101"), ("19940101")),
PARTITION p3 VALUES [("19940101"), ("19950101")),
PARTITION p4 VALUES [("19950101"), ("19960101")),
PARTITION p5 VALUES [("19960101"), ("19970101")),
PARTITION p6 VALUES [("19970101"), ("19980101")),
PARTITION p7 VALUES [("19980101"), ("19990101")))
DISTRIBUTED BY HASH(`lo_orderkey`) BUCKETS 48;

CREATE TABLE IF NOT EXISTS `customer` (
  `c_custkey` int(11) NOT NULL COMMENT "",
  `c_name` varchar(26) NOT NULL COMMENT "",
  `c_address` varchar(41) NOT NULL COMMENT "",
  `c_city` varchar(11) NOT NULL COMMENT "",
  `c_nation` varchar(16) NOT NULL COMMENT "",
  `c_region` varchar(13) NOT NULL COMMENT "",
  `c_phone` varchar(16) NOT NULL COMMENT "",
  `c_mktsegment` varchar(11) NOT NULL COMMENT ""
) ENGINE=OLAP
DUPLICATE KEY(`c_custkey`)
COMMENT "OLAP"
DISTRIBUTED BY HASH(`c_custkey`) BUCKETS 12;

CREATE TABLE IF NOT EXISTS `dates` (
  `d_datekey` int(11) NOT NULL COMMENT "",
  `d_date` varchar(20) NOT NULL COMMENT "",
  `d_dayofweek` varchar(10) NOT NULL COMMENT "",
  `d_month` varchar(11) NOT NULL COMMENT "",
  `d_year` int(11) NOT NULL COMMENT "",
  `d_yearmonthnum` int(11) NOT NULL COMMENT "",
  `d_yearmonth` varchar(9) NOT NULL COMMENT "",
  `d_daynuminweek` int(11) NOT NULL COMMENT "",
  `d_daynuminmonth` int(11) NOT NULL COMMENT "",
  `d_daynuminyear` int(11) NOT NULL COMMENT "",
  `d_monthnuminyear` int(11) NOT NULL COMMENT "",
  `d_weeknuminyear` int(11) NOT NULL COMMENT "",
  `d_sellingseason` varchar(14) NOT NULL COMMENT "",
  `d_lastdayinweekfl` int(11) NOT NULL COMMENT "",
  `d_lastdayinmonthfl` int(11) NOT NULL COMMENT "",
  `d_holidayfl` int(11) NOT NULL COMMENT "",
  `d_weekdayfl` int(11) NOT NULL COMMENT ""
) ENGINE=OLAP
DUPLICATE KEY(`d_datekey`)
COMMENT "OLAP"
DISTRIBUTED BY HASH(`d_datekey`) BUCKETS 1;

CREATE TABLE IF NOT EXISTS `supplier` (
  `s_suppkey` int(11) NOT NULL COMMENT "",
  `s_name` varchar(26) NOT NULL COMMENT "",
  `s_address` varchar(26) NOT NULL COMMENT "",
  `s_city` varchar(11) NOT NULL COMMENT "",
  `s_nation` varchar(16) NOT NULL COMMENT "",
  `s_region` varchar(13) NOT NULL COMMENT "",
  `s_phone` varchar(16) NOT NULL COMMENT ""
) ENGINE=OLAP
DUPLICATE KEY(`s_suppkey`)
COMMENT "OLAP"
DISTRIBUTED BY HASH(`s_suppkey`) BUCKETS 12;

CREATE TABLE IF NOT EXISTS `part` (
  `p_partkey` int(11) NOT NULL COMMENT "",
  `p_name` varchar(23) NOT NULL COMMENT "",
  `p_mfgr` varchar(7) NOT NULL COMMENT "",
  `p_category` varchar(8) NOT NULL COMMENT "",
  `p_brand` varchar(10) NOT NULL COMMENT "",
  `p_color` varchar(12) NOT NULL COMMENT "",
  `p_type` varchar(26) NOT NULL COMMENT "",
  `p_size` int(11) NOT NULL COMMENT "",
  `p_container` varchar(11) NOT NULL COMMENT ""
) ENGINE=OLAP
DUPLICATE KEY(`p_partkey`)
COMMENT "OLAP"
DISTRIBUTED BY HASH(`p_partkey`) BUCKETS 12;

create table orders ( 
    dt date NOT NULL, 
    order_id bigint NOT NULL, 
    user_id int NOT NULL, 
    merchant_id int NOT NULL, 
    good_id int NOT NULL, 
    good_name string NOT NULL, 
    price int NOT NULL, 
    cnt int NOT NULL, 
    revenue int NOT NULL, 
    state tinyint NOT NULL 
) 
PRIMARY KEY (dt, order_id) 
PARTITION BY RANGE(`dt`) 
( PARTITION p20210820 VALUES [('2021-08-20'), ('2021-08-21')), 
PARTITION p20210821 VALUES [('2021-08-21'), ('2021-08-22')) ) 
DISTRIBUTED BY HASH(order_id) BUCKETS 4 
PROPERTIES (
    "replication_num" = "1", 
    "enable_persistent_index" = "true"
);
```

Example 1: Create a non-partitioned materialized view.

```SQL
CREATE MATERIALIZED VIEW lo_mv1
DISTRIBUTED BY HASH(`lo_orderkey`) BUCKETS 10
REFRESH ASYNC
AS
select
    lo_orderkey, 
    lo_custkey, 
    sum(lo_quantity) as total_quantity, 
    sum(lo_revenue) as total_revenue, 
    count(lo_shipmode) as shipmode_count
from lineorder 
group by lo_orderkey, lo_custkey 
order by lo_orderkey;
```

Example 2: Create a partitioned materialized view.

```SQL
CREATE MATERIALIZED VIEW lo_mv2
PARTITION BY `lo_orderdate`
DISTRIBUTED BY HASH(`lo_orderkey`) BUCKETS 10
REFRESH ASYNC START('2023-07-01 10:00:00') EVERY (interval 1 day)
AS
select
    lo_orderkey,
    lo_orderdate,
    lo_custkey, 
    sum(lo_quantity) as total_quantity, 
    sum(lo_revenue) as total_revenue, 
    count(lo_shipmode) as shipmode_count
from lineorder 
group by lo_orderkey, lo_orderdate, lo_custkey
order by lo_orderkey;

-- Use the date_trunc() function to partition the materialized view by month.
CREATE MATERIALIZED VIEW order_mv1
PARTITION BY date_trunc('month', `dt`)
DISTRIBUTED BY HASH(`order_id`) BUCKETS 10
REFRESH ASYNC START('2023-07-01 10:00:00') EVERY (interval 1 day)
AS
select
    dt,
    order_id,
    user_id,
    sum(cnt) as total_cnt,
    sum(revenue) as total_revenue, 
    count(state) as state_count
from orders
group by dt, order_id, user_id;
```

Example 3: Create an asynchronous materialized view.

```SQL
CREATE MATERIALIZED VIEW flat_lineorder
DISTRIBUTED BY HASH(`lo_orderkey`) BUCKETS 48
REFRESH MANUAL
AS
SELECT
    l.LO_ORDERKEY AS LO_ORDERKEY,
    l.LO_LINENUMBER AS LO_LINENUMBER,
    l.LO_CUSTKEY AS LO_CUSTKEY,
    l.LO_PARTKEY AS LO_PARTKEY,
    l.LO_SUPPKEY AS LO_SUPPKEY,
    l.LO_ORDERDATE AS LO_ORDERDATE,
    l.LO_ORDERPRIORITY AS LO_ORDERPRIORITY,
    l.LO_SHIPPRIORITY AS LO_SHIPPRIORITY,
    l.LO_QUANTITY AS LO_QUANTITY,
    l.LO_EXTENDEDPRICE AS LO_EXTENDEDPRICE,
    l.LO_ORDTOTALPRICE AS LO_ORDTOTALPRICE,
    l.LO_DISCOUNT AS LO_DISCOUNT,
    l.LO_REVENUE AS LO_REVENUE,
    l.LO_SUPPLYCOST AS LO_SUPPLYCOST,
    l.LO_TAX AS LO_TAX,
    l.LO_COMMITDATE AS LO_COMMITDATE,
    l.LO_SHIPMODE AS LO_SHIPMODE,
    c.C_NAME AS C_NAME,
    c.C_ADDRESS AS C_ADDRESS,
    c.C_CITY AS C_CITY,
    c.C_NATION AS C_NATION,
    c.C_REGION AS C_REGION,
    c.C_PHONE AS C_PHONE,
    c.C_MKTSEGMENT AS C_MKTSEGMENT,
    s.S_NAME AS S_NAME,
    s.S_ADDRESS AS S_ADDRESS,
    s.S_CITY AS S_CITY,
    s.S_NATION AS S_NATION,
    s.S_REGION AS S_REGION,
    s.S_PHONE AS S_PHONE,
    p.P_NAME AS P_NAME,
    p.P_MFGR AS P_MFGR,
    p.P_CATEGORY AS P_CATEGORY,
    p.P_BRAND AS P_BRAND,
    p.P_COLOR AS P_COLOR,
    p.P_TYPE AS P_TYPE,
    p.P_SIZE AS P_SIZE,
    p.P_CONTAINER AS P_CONTAINER FROM lineorder AS l 
INNER JOIN customer AS c ON c.C_CUSTKEY = l.LO_CUSTKEY
INNER JOIN supplier AS s ON s.S_SUPPKEY = l.LO_SUPPKEY
INNER JOIN part AS p ON p.P_PARTKEY = l.LO_PARTKEY;
```

Example 5: Create a synchronous sync materialized views.

Base table schema is as follows:

```Plain Text
mysql> desc duplicate_table;
+-------+--------+------+------+---------+-------+
| Field | Type   | Null | Key  | Default | Extra |
+-------+--------+------+------+---------+-------+
| k1    | INT    | Yes  | true | N/A     |       |
| k2    | INT    | Yes  | true | N/A     |       |
| k3    | BIGINT | Yes  | true | N/A     |       |
| k4    | BIGINT | Yes  | true | N/A     |       |
+-------+--------+------+------+---------+-------+
```

1. Create a materialized view that only contains the columns of the original table (k1, k2).

    ```sql
    create materialized view k1_k2 as
    select k1, k2 from duplicate_table;
    ```

    The materialized view contains only two columns k1, k2 without any aggregation.

    ```plain text
    +-----------------+-------+--------+------+------+---------+-------+
    | IndexName       | Field | Type   | Null | Key  | Default | Extra |
    +-----------------+-------+--------+------+------+---------+-------+
    | k1_k2           | k1    | INT    | Yes  | true | N/A     |       |
    |                 | k2    | INT    | Yes  | true | N/A     |       |
    +-----------------+-------+--------+------+------+---------+-------+
    ```

2. Create a materialized view sorted by k2.

    ```sql
    create materialized view k2_order as
    select k2, k1 from duplicate_table order by k2;
    ```

    The materialized view's schema is shown below. The materialized view contains only two columns k2, k1, where column k2 is a sort column without any aggregation.

    ```plain text
    +-----------------+-------+--------+------+-------+---------+-------+
    | IndexName       | Field | Type   | Null | Key   | Default | Extra |
    +-----------------+-------+--------+------+-------+---------+-------+
    | k2_order        | k2    | INT    | Yes  | true  | N/A     |       |
    |                 | k1    | INT    | Yes  | false | N/A     | NONE  |
    +-----------------+-------+--------+------+-------+---------+-------+
    ```

3. Create a materialized view grouped by k1, k2 with k3 as the SUM aggregate.

    ```sql
    create materialized view k1_k2_sumk3 as
    select k1, k2, sum(k3) from duplicate_table group by k1, k2;
    ```

    The materialized view's schema is shown below. The materialized view contains two columns k1, k2 and sum (k3), where k1, k2 are grouped columns, and sum (k3) is the sum of the k3 columns grouped according to k1, k2.

    ```plain text
    +-----------------+-------+--------+------+-------+---------+-------+
    | IndexName       | Field | Type   | Null | Key   | Default | Extra |
    +-----------------+-------+--------+------+-------+---------+-------+
    | k1_k2_sumk3     | k1    | INT    | Yes  | true  | N/A     |       |
    |                 | k2    | INT    | Yes  | true  | N/A     |       |
    |                 | k3    | BIGINT | Yes  | false | N/A     | SUM   |
    +-----------------+-------+--------+------+-------+---------+-------+
    ```

    Because the materialized view does not declare a sort column, and the materialized view has aggregate data, the system supplements the grouped columns k1 and k2 by default.

4. Create a materialized view to remove duplicate rows.

    ```sql
    create materialized view deduplicate as
    select k1, k2, k3, k4 from duplicate_table group by k1, k2, k3, k4;
    ```

    The materialized view's schema is shown below. The materialized view contains k1, k2, k3, and k4 columns, and there are no duplicate rows.

    ```plain text
    +-----------------+-------+--------+------+-------+---------+-------+
    | IndexName       | Field | Type   | Null | Key   | Default | Extra |
    +-----------------+-------+--------+------+-------+---------+-------+
    | deduplicate     | k1    | INT    | Yes  | true  | N/A     |       |
    |                 | k2    | INT    | Yes  | true  | N/A     |       |
    |                 | k3    | BIGINT | Yes  | true  | N/A     |       |
    |                 | k4    | BIGINT | Yes  | true  | N/A     |       |
    +-----------------+-------+--------+------+-------+---------+-------+
    
    ```

5. Create a non-aggregated materialized view that does not declare a sort column.

    The schema of all_type_table is shown below:

    ```plain text
    +-------+--------------+------+-------+---------+-------+
    | Field | Type         | Null | Key   | Default | Extra |
    +-------+--------------+------+-------+---------+-------+
    | k1    | TINYINT      | Yes  | true  | N/A     |       |
    | k2    | SMALLINT     | Yes  | true  | N/A     |       |
    | k3    | INT          | Yes  | true  | N/A     |       |
    | k4    | BIGINT       | Yes  | true  | N/A     |       |
    | k5    | DECIMAL(9,0) | Yes  | true  | N/A     |       |
    | k6    | DOUBLE       | Yes  | false | N/A     | NONE  |
    | k7    | VARCHAR(20)  | Yes  | false | N/A     | NONE  |
    +-------+--------------+------+-------+---------+-------+
    ```

    The materialized view contains k3, k4, k5, k6, k7 columns, and no sort column is declared. The creation statement is as follows:

    ```sql
    create materialized view mv_1 as
    select k3, k4, k5, k6, k7 from all_type_table;
    ```

    The system's default supplementary sort columns are k3, k4, and k5. The sum of the number of bytes for these three column types is 4 (INT) + 8 (BIGINT) + 16 (DECIMAL) = 28 < 36. So these three columns are added as sort columns.

    The materialized view's schema is as follows.

    ```plain text
    +----------------+-------+--------------+------+-------+---------+-------+
    | IndexName      | Field | Type         | Null | Key   | Default | Extra |
    +----------------+-------+--------------+------+-------+---------+-------+
    | mv_1           | k3    | INT          | Yes  | true  | N/A     |       |
    |                | k4    | BIGINT       | Yes  | true  | N/A     |       |
    |                | k5    | DECIMAL(9,0) | Yes  | true  | N/A     |       |
    |                | k6    | DOUBLE       | Yes  | false | N/A     | NONE  |
    |                | k7    | VARCHAR(20)  | Yes  | false | N/A     | NONE  |
    +----------------+-------+--------------+------+-------+---------+-------+
    ```

    It can be observed that the key fields of the k3, k4, and k5 columns are true, which is the sort order. The key field of the k6, k7 columns is false, which is the non-sort order.
