-- $Header: /cvsroot/pgpool/pgpool-II/sql/insert_lock.sql,v 1.1 2011/07/01 10:47:14 kitagawa Exp $

DROP TABLE pgpool_catalog.insert_lock;

CREATE SCHEMA pgpool_catalog;
CREATE TABLE pgpool_catalog.insert_lock(reloid OID PRIMARY KEY);
 
-- this row is used as the row lock target when pgpool inserts new oid
INSERT INTO pgpool_catalog.insert_lock VALUES (0);

-- allow "SELECT ... FOR UPDATE" and "INSERT ..." to all roles
GRANT SELECT ON pgpool_catalog.insert_lock TO PUBLIC;
GRANT UPDATE ON pgpool_catalog.insert_lock TO PUBLIC;
GRANT INSERT ON pgpool_catalog.insert_lock TO PUBLIC;
