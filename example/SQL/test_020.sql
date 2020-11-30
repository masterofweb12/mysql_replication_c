

ALTER TABLE `tbl_1`
DROP COLUMN `double_val`,
DROP COLUMN `int_val`,
ADD COLUMN `geom` GEOMETRY NULL DEFAULT NULL AFTER `char_val`;



INSERT INTO `tbl_1` ( `id`, `char_val`, `geom` )
VALUES
 ( NULL, 'point_1', ST_GeomFromText( 'POINT( 1 1 )', 0 ) ),
 ( NULL, 'point_2', ST_GeomFromText( 'POINT( 2 2 )', 0 ) ),
 ( NULL, 'point_3', ST_GeomFromText( 'POINT( 3 3 )', 0 ) );


UPDATE `tbl_1` SET `char_val` = 'MEGAPOINT' WHERE `char_val` = 'point_1';

UPDATE `tbl_1` SET `geom` =  ST_GeomFromText( 'POINT( 50 70 )', 0 )  WHERE `char_val` = 'point_2';



DELETE FROM `tbl_1`;



