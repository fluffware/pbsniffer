<?php
function check_db_name($name)
{
  if (!preg_match("/^[a-zA-Z0-9_]+$/", $name)) {
    die ("Illegal database name");
  }

}

function check_signal_names($names)
{
  if (!$names) return;
  if (!preg_match("/^[a-zA-Z0-9_]+(,[a-zA-Z0-9_]+)*$/", $names)) {
    die ("Illegal signal name");
  }
}
?>
