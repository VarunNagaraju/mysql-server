--echo # WL#883 Non-recursive WITH clause (common table expression)

flush status;

create table t1(a int, b int, c int);
insert into t1 values(null,null,null),(2,3,4);

WITH qn AS (SELECT a FROM t1)
  SELECT 1 FROM dual;

--echo # two query names

WITH qn AS (SELECT a FROM t1), qn2 as (select b from t1)
SELECT 1 FROM dual;

--echo # duplicate query names

--error ER_NONUNIQ_TABLE
WITH qn AS (SELECT a FROM t1), qn as (select b from t1)
SELECT 1 FROM qn;

--echo #  multiple refs

--sorted_result
WITH qn AS (SELECT b as a FROM t1)
SELECT qn.a, qn2.a  FROM qn, qn as qn2;

--sorted_result
WITH qn AS (SELECT b as a FROM t1),
qn2 AS (SELECT c FROM t1 WHERE a IS NULL or a>0)
SELECT qn.a, qn2.c  FROM qn, qn2;

--echo # qn2 ref qn:

WITH qn AS (SELECT 10*a as a FROM t1),
qn2 AS (SELECT 3*a FROM qn)
SELECT * from qn2;

WITH qn AS (SELECT a FROM t1), qn2 AS (SELECT a FROM qn)
SELECT * from qn2;

let $query=
WITH qn AS (SELECT b as a FROM t1),
qn2 AS (SELECT a FROM qn WHERE a IS NULL or a>0)
SELECT qn.a, qn2.a  FROM qn, qn2;

--sorted_result
eval $query;
eval EXPLAIN $query;

--echo # forward ref (should error)

--error ER_NO_SUCH_TABLE
WITH qn2 AS (SELECT a FROM qn WHERE a IS NULL or a>0),
qn AS (SELECT b as a FROM t1)
SELECT qn2.a  FROM qn2;

--error ER_NO_SUCH_TABLE
with qn1 as (with qn3 as (select * from qn2) select * from qn3),
     qn2 as (select 1)
select * from qn1;

--echo # This is valid; it is to test moving boundaries.
--echo # When we resolve qn3, resolving qn1 moves the right bound to
--echo # qn0, but the bound is properly restored so that we can later
--echo # resolve qn2.
with qn0 as (select 1), qn1 as (select * from qn0), qn2 as (select 1), qn3 as (select 1 from qn1, qn2) select 1 from qn3;

--echo # No ref

explain with qn as (select 1) select 2;
with qn as (select 1) select 2;

--echo # circular ref

--error ER_NO_SUCH_TABLE
WITH qn2 AS (SELECT a FROM qn WHERE a IS NULL or a>0),
qn AS (SELECT b as a FROM qn2)
SELECT qn.a  FROM qn;

--echo # recursive

--error ER_NO_SUCH_TABLE
WITH qn AS (SELECT a FROM qn)
SELECT qn.a FROM qn;

--error ER_NO_SUCH_TABLE
WITH qn1 AS (SELECT a FROM qn3),
qn2 AS (SELECT a FROM qn1),
qn3 AS (SELECT a FROM t1),
qn4 AS (SELECT a FROM qn2)
SELECT a FROM qn4;

--echo # ref from subq

with qn as (select * from t1) select (select max(a) from qn);  

--echo # QN defined in subq

SELECT (WITH qn AS (SELECT 10*a as a FROM t1),
        qn2 AS (SELECT 3*a AS b FROM qn)
        SELECT * from qn2 LIMIT 1)
FROM t1;

SELECT *
FROM (WITH qn AS (SELECT 10*a as a FROM t1),
      qn2 AS (SELECT 3*a AS b FROM qn)
      SELECT * from qn2)
AS dt;

--echo # WITH in WITH
with qn as
  (with qn2 as (select "qn2" as a from t1) select "qn", a from qn2)
select * from qn;

--echo # outer ref to a table, placed in a QN in a subq (later)
if (0)
{
SELECT (WITH qn AS (SELECT t2.a*a as a FROM t1),
        qn2 AS (SELECT 3*a AS b FROM qn)
        SELECT * from qn2 LIMIT 1)
FROM t2 as t1;

--echo # outer ref to a QN, placed in a QN in a subq

WITH qn AS (SELECT b as a FROM t1)
SELECT (WITH qn2 AS (SELECT a FROM qn WHERE a IS NULL or a>0)
        SELECT qn2.a FROM qn2) FROM qn;
}

--echo # QN defined in view

CREATE VIEW v AS
WITH qn AS (SELECT 10*a as a FROM t1),
      qn2 AS (SELECT 3*a AS b FROM qn)
      SELECT * from qn2;
SELECT * FROM v;
DROP VIEW v;

--echo # CREATE INSERT SELECT

CREATE TABLE t2
WITH qn AS (SELECT 10*a as a FROM t1),
      qn2 AS (SELECT 3*a AS b FROM qn)
      SELECT * from qn2;
SELECT * FROM t2;
INSERT INTO t2
WITH qn AS (SELECT 10*a as a FROM t1),
      qn2 AS (SELECT 3*a AS b FROM qn)
      SELECT * from qn2;
SELECT * FROM t2;
DROP TABLE t2;

--echo # Double use of QN in two subqueries.

let $query=
with qn as (select * from t1 limit 10)
select (select max(a) from qn where a=0),
       (select min(b) from qn where b=3);
eval explain $query;
eval $query;

--echo # when QN, when table.
create table qn select "base";

select * from qn;

with qn as (select "with") select * from qn;

--echo # In a non-recursive WITH, the scope of the QN doesn't extend to its
--echo # subquery, so "qn" inside AS() is the base table.
WITH qn AS (select * from qn) select * from qn;

--echo # View doesn't look out to external QNs

create view v as select * from qn;

select * from v;

with qn as (select "with") select * from v;

with qn as (select * from v) select * from qn;

--echo # Even if the base table is temporarily dropped
drop table qn;
--error ER_VIEW_INVALID
with qn as (select "with") select * from v;
--error ER_VIEW_INVALID
with qn as (select * from v) select * from qn;
create table qn select "base" as a;

--echo # Neither does SP

create function f() returns varchar(10)
 return (select * from qn);

select f();

with qn as (select "with") select f();

with qn as (select f()) select * from qn;

--echo # QN shadows tmp table

create temporary table qn select "tmp" as a;

select * from qn;

with qn as (select "with") select * from qn;

drop function f;
drop view v;

--echo # DT shadows QN:

with qn as (select "with") select * from (select "dt") as qn;

--echo # QN of subq shadows outer QN
WITH qn AS (select "outer" as a)
SELECT (WITH qn AS (SELECT "inner" as a) SELECT a from qn),
       qn.a
FROM qn;

--echo # Qualified name isn't allowed after WITH:

--error ER_PARSE_ERROR
with test.qn as (select "with") select * from test.qn;

--echo # Adding a database prefix to a field does not resolve to the QN:
--error ER_BAD_FIELD_ERROR
select test.qn.a from (select "with" as a) qn;

--echo # database prefix in FROM does not resolve to QN:
with qn as (select "with") select * from qn;
with qn as (select "with") select * from test.qn;
with qn as (select "with" as a) select a from qn;
with qn as (select "with" as a) select qn.a from qn;
--error ER_BAD_FIELD_ERROR
with qn as (select "with" as a) select test.qn.a from qn;
with qn as (select "with" as a) select a from test.qn;
with qn as (select "with" as a) select qn.a from test.qn;
with qn as (select "with" as a) select test.qn.a from test.qn;

drop temporary table qn;

with qn as (select "with" as a) select a from test.qn;
with qn as (select "with" as a) select qn.a from test.qn;
with qn as (select "with" as a) select test.qn.a from test.qn;

drop table qn;

--error ER_NO_SUCH_TABLE
with qn as (select "with" as a) select a from test.qn;
--error ER_NO_SUCH_TABLE
with qn as (select "with" as a) select qn.a from test.qn;
--error ER_NO_SUCH_TABLE
with qn as (select "with" as a) select test.qn.a from test.qn;

--echo # Unions

WITH qn AS (SELECT b as a FROM t1 UNION SELECT b+5 FROM t1),
qn2 AS (SELECT a FROM qn WHERE a IS NULL or a>0)
SELECT qn.a FROM qn
UNION SELECT qn2.a FROM qn2 WHERE qn2.a>3;

--echo # No double WITH

--error ER_PARSE_ERROR
with qn as (select "with" as a)
with qn2 as (select "with" as a)
select a from test.qn;
--echo # with comma
--error ER_PARSE_ERROR
with qn as (select "with" as a),
with qn2 as (select "with" as a)
select a from test.qn;

--echo # ORDER BY removed unless there is LIMIT or single table (check "Using filesort")
explain
with qn as (select a from t1 order by 1)
select a from qn;

explain
with qn as (select a from t1 order by 1)
select qn.a from qn, t1 as t2;

explain
with qn as (select a from t1 order by 1 limit 10)
select qn.a from qn, t1 as t2;

--echo # Merge hint

explain
with qn as (select a from t1),
     qn2 as (select b from t1)
select /*+ merge(qn) no_merge(qn2) */ qn.a,qn2.b from qn, qn2;

explain
with qn as (select a from t1)
select /*+ merge(qn) no_merge(qn2) */ qn2.a from qn, qn as qn2;

--echo # FD detection

--error ER_WRONG_FIELD_WITH_GROUP
with qn as (select a, b from t1)
select b from qn group by a;

with qn as (select a, b from t1 where a=b)
select b from qn group by a;

with qn as (select a, sum(b) as s from t1 group by a)
select s from qn group by a;

--echo # CTEs work if used in SET

set @myvar=
(with qn as (select a, sum(b) as s from t1 group by a)
select s from qn group by a having s is not null);
select @myvar;

--echo # CTE works with semijoin
let $query=
with cte as (select * from t1 as t2 limit 1)
select * from t1 where t1.a in (select a+0 from cte);
eval explain $query;
eval $query;

let $query=
with cte as (select * from t1 as t2)
select * from t1 where t1.a in (select a+0 from cte);
eval explain $query;
eval $query;

--echo # Column names

--echo # empty list
--error ER_PARSE_ERROR
with qn () as (select 1) select * from qn, qn qn1;
--echo # Materialization
--error ER_VIEW_WRONG_LIST
with qn (foo, bar) as (select 1) select * from qn, qn qn1;
explain with qn (foo, bar) as (select 1, 2 from t1 limit 2) select * from qn, qn qn1;
with qn (foo, bar) as (select 1, 2 from t1 limit 2) select * from qn, qn qn1;
with qn (foo, bar) as (select 1 as col, 2 as coll from t1 limit 2) select * from qn, qn qn1;
with qn (foo, bar) as (select 1 as col, 2 as coll union
                       select a,b from t1) select qn1.bar from qn qn1;
with qn (foo, bar) as (select a, b from t1 limit 2) select qn.bar,foo from qn;
create table t3
with qn (foo, bar) as (select a, b from t1 limit 2) select bar,foo from qn;
desc t3;
drop table t3;

--echo # Merge

--error ER_VIEW_WRONG_LIST
with qn (foo, bar) as (select 1 from t1) select * from qn, qn qn1;
with qn (foo, bar) as (select 1, 2 from t1) select * from qn, qn qn1;
explain with qn (foo, bar) as (select 1, 2 from t1) select * from qn, qn qn1;
with qn (foo, bar) as (select 1 as col, 2 as coll from t1) select * from qn, qn qn1;
with qn (foo, bar) as (select a, b from t1) select qn1.bar,foo from qn qn1;
create table t3
with qn (foo, bar) as (select a, b from t1) select bar,foo from qn;
desc t3;
drop table t3;

--echo # Disambiguates same-name expressions
--error ER_DUP_FIELDNAME
with qn as (select 1,1) select * from qn;
with qn (foo, bar) as (select 1,1) select * from qn;
--error ER_DUP_FIELDNAME
with qn as (select 1,1 from t1) select * from qn;
with qn (foo, bar) as (select 1,1 from t1) select * from qn;
--echo # Duplicate names are forbidden
--error ER_DUP_FIELDNAME
with qn (foo, foo) as (select 1,2) select * from qn;

--echo # Derived tables support this too
select * from (select '1', 1) dt(foo,bar);
select * from (select a,b from t1) dt(foo,bar);
--error ER_VIEW_WRONG_LIST
select * from (select a from t1) dt(foo,bar);

--echo # Column names for QN/DT are printed
create view v1 as
 with qn (foo, bar) as (select 1,1) select * from qn;
show create view v1;
show fields from v1;
select * from v1;
drop view v1;
create view v1 as
 select * from (select 1,1) dt(foo,bar);
show create view v1;
select * from v1;
drop view v1;
create view v1 as
 with qn (foo, bar) as (select 1,1 from t1) select * from qn;
show create view v1;
select * from v1;
drop view v1;
create view v1 as
 select * from (select 1,1 from t1) dt(foo,bar);
show create view v1;
select * from v1;
drop view v1;
--echo # printing with back-quoting is necessary, when using a
--echo # reserved word as column name.
create view v1 as
 select * from (select 1) dt(`select`);
show create view v1;
select * from v1;
drop view v1;

--echo # Works for views too. Using testcase of:
--echo # Bug#23265335 SPECIFYING A NAME FOR VIEW'S COLUMN IN CREATE VIEW MAKES SELECT FAIL
create view v1 (bar) as
 select 1 as foo group by foo union select 2 order by foo;
select * from v1;
show create view v1;
--echo # The column's name for the view
select TABLE_NAME,COLUMN_NAME from information_schema.columns
where TABLE_SCHEMA='test' and TABLE_NAME='v1';
--echo # is different from the alias in the defining SELECT
select VIEW_DEFINITION from information_schema.views
where TABLE_SCHEMA='test' and TABLE_NAME='v1';

drop view v1;

--error ER_VIEW_WRONG_LIST
create view v1 (bar) as
 select 1, 2 from t1;

drop table t1;

--echo # Prove that a materialized QN is shared among all references:
create table t1(a int);
insert into t1 values(1),(2),(3),(4);
flush status;
with qn as (select 123 as col)
select * from qn;
show status like "handler_write";
flush status;
with qn as (select 123 as col)
select * from qn, qn as qn1;
show status like "handler_write";
# Contrast that with view:
create view qn as select 123 as col;
flush status;
select * from qn, qn as qn1;
show status like "handler_write";
drop view qn;

drop table t1;

--echo # Printing of WITH to DD for view
create view v as
select (with qn as (select "with") select * from qn) as scal_subq
from dual;
show create view v;
select * from v;
drop view v;
create view v as select * from (with qn as (select "with") select * from qn) as dt;
show create view v;
select * from v;
drop view v;

--echo # Printing of merged/materialized QN, with or without alias

create table t1 (a int);

let $query=
with qne as (select a from t1),
     qnm as (select a from t1),
     qnea as (select a from t1),
     qnma as (select a from t1)
select /*+ merge(qne) no_merge(qnm) merge(alias1) no_merge(alias2) */
qne.a,qnm.a,alias1.a,alias2.a
from qne, qnm, qnea as alias1, qnma as alias2;

eval explain $query;
drop table t1;

--echo # Automatic index creation if materialized
create table t1 (a int);

insert into t1(a) values (1),(2),(3),(4),(5),(6),(7),(8),(9),(0);
analyze table t1;

let $query=
with tt as (select * from t1)
select /*+ no_merge(tt) */ tt.a
from t1 straight_join tt where t1.a=tt.a
limit 1;

--echo # EXPLAIN should not fill the tmp table
flush status;

--echo # Should use auto_key0 and ref access.
--replace_column 10 #
eval explain $query;
show status like "handler_write";

flush status;
eval $query;
show status like "handler_write";

--echo # With two references
let $query=
with tt as (select * from t1)
select /*+ no_merge(tt) no_merge(tt_)*/ tt.a
from t1 straight_join tt straight_join tt as tt_
where t1.a=tt.a and tt.a=tt_.a
limit 1;

eval $query;

--echo # One merged, one materialized: index creation on the second
--echo # should of course ignore the first
with q as (select * from t1)
select /*+ merge(q) no_merge(q1) */ * from q, q q1 where q.a=1 and q1.a=2;

drop table t1;

--echo # Must not create more than 64 indexes.

--disable_query_log
CREATE TABLE `t` (
  `c1` int(11) DEFAULT NULL,
  `c2` int(11) DEFAULT NULL,
  `c3` int(11) DEFAULT NULL,
  `c4` int(11) DEFAULT NULL,
  `c5` int(11) DEFAULT NULL,
  `c6` int(11) DEFAULT NULL,
  `c7` int(11) DEFAULT NULL,
  `c8` int(11) DEFAULT NULL,
  `c9` int(11) DEFAULT NULL,
  `c10` int(11) DEFAULT NULL,
  `c11` int(11) DEFAULT NULL,
  `c12` int(11) DEFAULT NULL,
  `c13` int(11) DEFAULT NULL,
  `c14` int(11) DEFAULT NULL,
  `c15` int(11) DEFAULT NULL,
  `c16` int(11) DEFAULT NULL,
  `c17` int(11) DEFAULT NULL,
  `c18` int(11) DEFAULT NULL,
  `c19` int(11) DEFAULT NULL,
  `c20` int(11) DEFAULT NULL,
  `c21` int(11) DEFAULT NULL,
  `c22` int(11) DEFAULT NULL,
  `c23` int(11) DEFAULT NULL,
  `c24` int(11) DEFAULT NULL,
  `c25` int(11) DEFAULT NULL,
  `c26` int(11) DEFAULT NULL,
  `c27` int(11) DEFAULT NULL,
  `c28` int(11) DEFAULT NULL,
  `c29` int(11) DEFAULT NULL,
  `c30` int(11) DEFAULT NULL,
  `c31` int(11) DEFAULT NULL,
  `c32` int(11) DEFAULT NULL,
  `c33` int(11) DEFAULT NULL,
  `c34` int(11) DEFAULT NULL,
  `c35` int(11) DEFAULT NULL,
  `c36` int(11) DEFAULT NULL,
  `c37` int(11) DEFAULT NULL,
  `c38` int(11) DEFAULT NULL,
  `c39` int(11) DEFAULT NULL,
  `c40` int(11) DEFAULT NULL,
  `c41` int(11) DEFAULT NULL,
  `c42` int(11) DEFAULT NULL,
  `c43` int(11) DEFAULT NULL,
  `c44` int(11) DEFAULT NULL,
  `c45` int(11) DEFAULT NULL,
  `c46` int(11) DEFAULT NULL,
  `c47` int(11) DEFAULT NULL,
  `c48` int(11) DEFAULT NULL,
  `c49` int(11) DEFAULT NULL,
  `c50` int(11) DEFAULT NULL,
  `c51` int(11) DEFAULT NULL,
  `c52` int(11) DEFAULT NULL,
  `c53` int(11) DEFAULT NULL,
  `c54` int(11) DEFAULT NULL,
  `c55` int(11) DEFAULT NULL,
  `c56` int(11) DEFAULT NULL,
  `c57` int(11) DEFAULT NULL,
  `c58` int(11) DEFAULT NULL,
  `c59` int(11) DEFAULT NULL,
  `c60` int(11) DEFAULT NULL,
  `c61` int(11) DEFAULT NULL,
  `c62` int(11) DEFAULT NULL,
  `c63` int(11) DEFAULT NULL,
  `c64` int(11) DEFAULT NULL,
  `c65` int(11) DEFAULT NULL,
  `c66` int(11) DEFAULT NULL,
  `c67` int(11) DEFAULT NULL,
  `c68` int(11) DEFAULT NULL,
  `c69` int(11) DEFAULT NULL,
  `c70` int(11) DEFAULT NULL,
  `c71` int(11) DEFAULT NULL,
  `c72` int(11) DEFAULT NULL,
  `c73` int(11) DEFAULT NULL,
  `c74` int(11) DEFAULT NULL,
  `c75` int(11) DEFAULT NULL,
  `c76` int(11) DEFAULT NULL,
  `c77` int(11) DEFAULT NULL,
  `c78` int(11) DEFAULT NULL,
  `c79` int(11) DEFAULT NULL,
  `c80` int(11) DEFAULT NULL,
  `c81` int(11) DEFAULT NULL,
  `c82` int(11) DEFAULT NULL,
  `c83` int(11) DEFAULT NULL,
  `c84` int(11) DEFAULT NULL,
  `c85` int(11) DEFAULT NULL,
  `c86` int(11) DEFAULT NULL,
  `c87` int(11) DEFAULT NULL,
  `c88` int(11) DEFAULT NULL,
  `c89` int(11) DEFAULT NULL,
  `c90` int(11) DEFAULT NULL,
  `c91` int(11) DEFAULT NULL,
  `c92` int(11) DEFAULT NULL,
  `c93` int(11) DEFAULT NULL,
  `c94` int(11) DEFAULT NULL,
  `c95` int(11) DEFAULT NULL,
  `c96` int(11) DEFAULT NULL,
  `c97` int(11) DEFAULT NULL,
  `c98` int(11) DEFAULT NULL,
  `c99` int(11) DEFAULT NULL,
  `c100` int(11) DEFAULT NULL
);

with qn as (select * from t limit 2)
select
(select max(c1) from qn where qn.c1=1),
(select max(c2) from qn where qn.c2=1),
(select max(c3) from qn where qn.c3=1),
(select max(c4) from qn where qn.c4=1),
(select max(c5) from qn where qn.c5=1),
(select max(c6) from qn where qn.c6=1),
(select max(c7) from qn where qn.c7=1),
(select max(c8) from qn where qn.c8=1),
(select max(c9) from qn where qn.c9=1),
(select max(c10) from qn where qn.c10=1),
(select max(c11) from qn where qn.c11=1),
(select max(c12) from qn where qn.c12=1),
(select max(c13) from qn where qn.c13=1),
(select max(c14) from qn where qn.c14=1),
(select max(c15) from qn where qn.c15=1),
(select max(c16) from qn where qn.c16=1),
(select max(c17) from qn where qn.c17=1),
(select max(c18) from qn where qn.c18=1),
(select max(c19) from qn where qn.c19=1),
(select max(c20) from qn where qn.c20=1),
(select max(c21) from qn where qn.c21=1),
(select max(c22) from qn where qn.c22=1),
(select max(c23) from qn where qn.c23=1),
(select max(c24) from qn where qn.c24=1),
(select max(c25) from qn where qn.c25=1),
(select max(c26) from qn where qn.c26=1),
(select max(c27) from qn where qn.c27=1),
(select max(c28) from qn where qn.c28=1),
(select max(c29) from qn where qn.c29=1),
(select max(c30) from qn where qn.c30=1),
(select max(c31) from qn where qn.c31=1),
(select max(c32) from qn where qn.c32=1),
(select max(c33) from qn where qn.c33=1),
(select max(c34) from qn where qn.c34=1),
(select max(c35) from qn where qn.c35=1),
(select max(c36) from qn where qn.c36=1),
(select max(c37) from qn where qn.c37=1),
(select max(c38) from qn where qn.c38=1),
(select max(c39) from qn where qn.c39=1),
(select max(c40) from qn where qn.c40=1),
(select max(c41) from qn where qn.c41=1),
(select max(c42) from qn where qn.c42=1),
(select max(c43) from qn where qn.c43=1),
(select max(c44) from qn where qn.c44=1),
(select max(c45) from qn where qn.c45=1),
(select max(c46) from qn where qn.c46=1),
(select max(c47) from qn where qn.c47=1),
(select max(c48) from qn where qn.c48=1),
(select max(c49) from qn where qn.c49=1),
(select max(c50) from qn where qn.c50=1),
(select max(c51) from qn where qn.c51=1),
(select max(c52) from qn where qn.c52=1),
(select max(c53) from qn where qn.c53=1),
(select max(c54) from qn where qn.c54=1),
(select max(c55) from qn where qn.c55=1),
(select max(c56) from qn where qn.c56=1),
(select max(c57) from qn where qn.c57=1),
(select max(c58) from qn where qn.c58=1),
(select max(c59) from qn where qn.c59=1),
(select max(c60) from qn where qn.c60=1),
(select max(c61) from qn where qn.c61=1),
(select max(c62) from qn where qn.c62=1),
(select max(c63) from qn where qn.c63=1),
(select max(c64) from qn where qn.c64=1),
(select max(c65) from qn where qn.c65=1),
(select max(c66) from qn where qn.c66=1),
(select max(c67) from qn where qn.c67=1),
(select max(c68) from qn where qn.c68=1),
(select max(c69) from qn where qn.c69=1),
(select max(c70) from qn where qn.c70=1),
(select max(c71) from qn where qn.c71=1),
(select max(c72) from qn where qn.c72=1),
(select max(c73) from qn where qn.c73=1),
(select max(c74) from qn where qn.c74=1),
(select max(c75) from qn where qn.c75=1),
(select max(c76) from qn where qn.c76=1),
(select max(c77) from qn where qn.c77=1),
(select max(c78) from qn where qn.c78=1),
(select max(c79) from qn where qn.c79=1),
(select max(c80) from qn where qn.c80=1),
(select max(c81) from qn where qn.c81=1),
(select max(c82) from qn where qn.c82=1),
(select max(c83) from qn where qn.c83=1),
(select max(c84) from qn where qn.c84=1),
(select max(c85) from qn where qn.c85=1),
(select max(c86) from qn where qn.c86=1),
(select max(c87) from qn where qn.c87=1),
(select max(c88) from qn where qn.c88=1),
(select max(c89) from qn where qn.c89=1),
(select max(c90) from qn where qn.c90=1),
(select max(c91) from qn where qn.c91=1),
(select max(c92) from qn where qn.c92=1),
(select max(c93) from qn where qn.c93=1),
(select max(c94) from qn where qn.c94=1),
(select max(c95) from qn where qn.c95=1),
(select max(c96) from qn where qn.c96=1),
(select max(c97) from qn where qn.c97=1),
(select max(c98) from qn where qn.c98=1),
(select max(c99) from qn where qn.c99=1),
(select max(c100) from qn where qn.c100=1)
from dual;
--enable_query_log

drop table t;

--echo # Choice between two auto_key:

create table t1(a int, b int);
insert into t1 values (null, 6), (null, 10);
let $query=
with t2 as
 (select * from t1)
SELECT /*+ no_merge(t2) */ * FROM t2
WHERE (a = a OR b <= 6) AND (a IS NULL);
--echo # Test the covering key; note that MEMORY doesn't use a
--echo # covering key (always reads the "data file"). But InnoDB does.
eval EXPLAIN $query;
eval $query;
drop table t1;

--echo # QN referencing view of same name isn't a "recursive view",
--echo # shouldn't cause ER_VIEW_RECURSIVE

create view v1 as select "with";
with v1 as (select * from v1) select * from v1;
drop view v1;

--echo # QN inside view

create view v1 as
with qn as (select 1 as col) select * from qn;
select * from v1;
drop view v1;
create table t1(a int, b int);

--echo # Alas merge hints are ignored in views (filed Bug#23017428)
create view v1 as
with qn as (select a from t1),
     qn2 as (select b from t1)
select /*+ merge(qn) no_merge(qn2) */ qn.a,qn2.b from qn, qn2;
explain select * from v1;
drop view v1;

--echo # Materializing view doesn't impose materializing query name
create algorithm=temptable view v1 as
with qn as (select a from t1)
select qn.a  from qn;
explain select * from v1;
drop view v1;

drop table t1;

--echo # CTE referenced four times, including in subqueries in other CTEs

# One row per day, with amount sold on that day:
create table sales_days(day_of_sale DATE, amount INT);
insert into sales_days values
('2015-01-02', 100), ('2015-01-05', 200),
('2015-02-02', 10),  ('2015-02-10', 100),
('2015-03-02', 10),  ('2015-03-18', 1);

with
 # first CTE: one row per month, with amount sold on all days of month
 sales_by_month(month,total) as
 (select month(day_of_sale), sum(amount) from sales_days
  where year(day_of_sale)=2015
  group by month(day_of_sale)),
 # second CTE: best month
 best_month(month, total, award) as
 (select month, total, "best" from sales_by_month
  where total=(select max(total) from sales_by_month)),
 # 3rd CTE: worst month
 worst_month(month, total, award) as
 (select month, total, "worst" from sales_by_month
  where total=(select min(total) from sales_by_month))
 # Now show results:
 select * from best_month union all select * from worst_month;

drop table sales_days;

--echo # Special parser command not allowed to users.
--error ER_PARSE_ERROR
parse_cte ( select 1 ) ;

--echo # Query names are a partial workaround to the problem that
--echo # user-created temp tables can't be referenced twice.

create temporary table tmp(a int) as select 1;
--error ER_CANT_REOPEN_TABLE
select * from tmp, tmp tmp1;

--echo # the workaround works if the temp table's life is necessary
--echo # only for a single statement:
with qn as (select 1) select * from qn, qn qn1;

--echo # If the tmp table is necessary, wrapping it in a query name doesn't
--echo # help:
--error ER_CANT_REOPEN_TABLE
with qn as (select * from tmp) select /*+ merge(qn,qn1) */ * from qn, qn qn1;
--error ER_CANT_REOPEN_TABLE
with qn as (select * from tmp) select /*+ no_merge(qn,qn1) */ * from qn, qn qn1;

drop temporary table tmp;

--echo # Using a query name in UPDATE

create table t1(a int, b int);
insert into t1 values(1,2),(3,4);
create table t2 select * from t1;
set autocommit=0;

--echo # Multi-table syntax

let $query=
with qn as (select a, b from t1) update t1, qn set qn.a=qn.a+10;
--error ER_NON_UPDATABLE_TABLE
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t1) update t1, qn set t1.a=qn.a+10 where t1.a-qn.a=0;
eval explain $query;
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t2) update t1, qn set t1.a=qn.a+10 where t1.a-qn.a=0;
eval explain $query;
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t2) update /*+ no_merge(qn) */ t1, qn set t1.a=qn.a+10 where t1.a-qn.a=0;
eval explain $query;
eval $query;
select * from t1;
rollback;

--echo # Two references to query name

let $query=
with qn as (select a+2 as a, b from t2)
update t1, qn, qn as qn2 set t1.a=qn.a+10 where t1.a-qn.a=0 and qn.b=qn2.b;
eval explain $query;
eval $query;
select * from t1;
rollback;

--echo # Single-table syntax

let $query=
with qn as (select a+2 as a, b from t2) update t1
  set t1.a=(select qn.a+10 from qn where t1.a-qn.a=0 limit 1);
eval explain $query;
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t2) update t1
  set t1.a=(select /*+ merge(qn) */ qn.a+10 from qn where t1.a-qn.a=0 limit 1);
eval explain $query;
eval $query;
select * from t1;
rollback;

--echo # Using a query name in DELETE

--echo # Multi-table syntax

let $query=
with qn as (select a, b from t1) delete qn from t1,qn;
--error ER_NON_UPDATABLE_TABLE
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t1) delete t1 from t1, qn where t1.a-qn.a=0;
eval explain $query;
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t2) delete t1 from t1, qn where t1.a-qn.a=0;
eval explain $query;
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t2) delete /*+ no_merge(qn) */ t1 from t1, qn where t1.a-qn.a=0;
eval explain $query;
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t2)
delete t1 from t1, qn, qn as qn2 where t1.a-qn.a=0 and qn.b=qn2.b;
eval explain $query;
eval $query;
select * from t1;
rollback;

--echo # Single-table syntax

let $query=
with qn as (select a+2 as a, b from t2)
  delete from t1 where t1.a=(select qn.a from qn where t1.a-qn.a=0 limit 1);
eval explain $query;
eval $query;
select * from t1;
rollback;
let $query=
with qn as (select a+2 as a, b from t2)
  delete from t1 where t1.a=(select /*+ merge(qn) */ qn.a from qn where t1.a-qn.a=0 limit 1);
eval explain $query;
eval $query;
select * from t1;
rollback;

drop table t1,t2;
set autocommit=default;

--echo # No default db

select database();
create database mysqltest1;
use mysqltest1;
drop database mysqltest1;
select database();

with qn as (select 1) select * from qn;

--echo # Back to usual db 'test'

use test;

--skip_if_hypergraph   # Depends on the query plan.
show status like 'Created_tmp_disk_tables';
