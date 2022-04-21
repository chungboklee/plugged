--TEST--
Bug #63447 (max_input_vars doesn't filter variables when mbstring.encoding_translation = On)
--EXTENSIONS--
mbstring
--INI--
max_input_nesting_level=10
max_input_vars=5
mbstring.encoding_translation=1
--POST--
a=1&b=2&c=3&d=4&e=5&f=6
--FILE--
<?php
var_dump($_POST);
?>
--EXPECT--
Warning: Unknown: Input variables exceeded 5. To increase the limit change max_input_vars in php.ini. in Unknown on line 0
array(0) {
}
