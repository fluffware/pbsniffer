CREATE TABLE test_log_values (id varchar(20) NOT NULL, start bigint NOT NULL, end bigint NOT NULL, value int, PRIMARY KEY (id, start), CHECK(start<end));
CREATE TABLE test_log_signals (id varchar(20) NOT NULL PRIMARY KEY, label varchar(50) DEFAULT NULL, src int, dest int, bit_offset int, bit_width int, type varchar(20));
