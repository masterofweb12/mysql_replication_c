

DELETE FROM `tbl_1`;

ALTER TABLE `tbl_1` ADD COLUMN  `jsn_data` json DEFAULT NULL;


INSERT INTO `tbl_1` ( `id`, `jsn_data` )
VALUES
 ( 1, '{"line_id": "1", "nazv": "begemot"}' ),
 ( 2, '{"line_id": "2", "nazv": "voland"}' ),
 ( 3, '{"line_id": "3", "nazv": "koroviev"}' );


UPDATE `tbl_1` SET `jsn_data` = JSON_SET( `jsn_data`, "$.nazv", "margarita" ) WHERE `id` = 2;
UPDATE `tbl_1` SET `jsn_data` = JSON_SET( `jsn_data`, "$.nazv", "master" ) WHERE `id` = 1;

INSERT INTO `tbl_1` ( `id`, `jsn_data` )
VALUES
 ( 7, '{"line_id": "7", "nazv": "-"}' ),
 ( 8, '{"line_id": "8", "nazv": "+"}' ),
 ( 9, '{"line_id": "9", "nazv": "*"}' );



DELETE FROM `tbl_1`;



