#
# Bug #51639
# This test verifies that 'do table'filters will follow
# the setting of lower_case_table_name to be case
# sensitive and accent sensitive when setting
# lower_case_table_name == 0
#

-- source include/rpl/init_source_replica.inc
-- source include/have_binlog_format_statement.inc

-- source common/rpl/rpl_filters.test 
let $replicate_do_table= query_get_value(SHOW REPLICA STATUS, Replicate_Do_Table, 1);
--echo Replicate_Do_Table: $replicate_do_table

--source include/rpl/deinit.inc
