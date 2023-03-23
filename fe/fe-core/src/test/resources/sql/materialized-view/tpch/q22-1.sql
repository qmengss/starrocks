[sql]
select
    sum(c_acctbal) / count(c_acctbal)
from
    customer
where
        c_acctbal > 0.00
  and substring(c_phone , 1  ,2)  in
      ('21', '28', '24', '32', '35', '34', '37');
[result]
AGGREGATE ([GLOBAL] aggregate [{74: sum=sum(74: sum), 75: count=sum(75: count)}] group by [[]] having [null]
    EXCHANGE GATHER
        AGGREGATE ([LOCAL] aggregate [{74: sum=sum(73: c_sum), 75: count=sum(72: c_count)}] group by [[]] having [null]
            SCAN (mv[customer_agg_mv1] columns[70: c_acctbal, 71: substring_phone, 72: c_count, 73: c_sum] predicate[70: c_acctbal > 0.00 AND 71: substring_phone IN (21, 28, 24, 32, 35, 34, 37)])
[end]
