
DROP TABLE IF EXISTS `tbl_1`;

CREATE TABLE `tbl_1` (
 `id` int unsigned NOT NULL AUTO_INCREMENT,
 `char_val` char(20) NOT NULL,
 `int_val` int NOT NULL,
 `double_val` DOUBLE NOT NULL,
 PRIMARY KEY (`id`)
) ENGINE=InnoDB;



INSERT INTO `tbl_1` (`id`, `char_val`, `int_val`, `double_val`)
VALUES
(NULL, 'first string', '10', '100.15'),
(NULL, 'second string', '20', '100.45');


INSERT INTO `tbl_1` (`id`, `char_val`, `int_val`, `double_val`)
VALUES
(NULL, 'apple', '30', '200.999'),
(NULL, 'peach', '40', '300.45');


INSERT INTO `tbl_1` (`id`, `char_val`, `int_val`, `double_val`)
VALUES
(NULL, 'car', '50', '400.888'),
(NULL, 'airplane', '60', '500.777'),
(NULL, 'ship', '70', '600.3456');



UPDATE `tbl_1` SET `int_val` = RAND()*1000;

UPDATE `tbl_1` SET `int_val` = 11 WHERE `double_val` > 200 AND `double_val` < 500;



DELETE FROM `tbl_1`;

