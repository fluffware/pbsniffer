<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE html PUBLIC
	  "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN"
	  "http://www.w3.org/2002/04/xhtml-math-svg/xhtml-math-svg.dtd">
<?php require 'log_utils.php'; ?>
<?php check_db_name($_GET['log']); ?>
<?php check_signal_names($_GET['signals']); ?>

<?php header('Content-Type: application/xhtml+xml; charset=utf-8'); ?>
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <link rel="stylesheet" type="text/css" href="graph.css"/>
    <script type="application/javascript" src="jquery_min.js">
    </script>
    <script type="application/javascript" src="graph.js">
    </script>
    <script type="application/javascript">
      
function start()
{
    graph = new SignalViewer("signal_graph", "/logs/test_log");
    step = graph.getStepper();
    var ca = graph.addCursor("a");
    var cb = graph.addCursor("b");
    var cta = $("#cursor-time-a");
    ca.moved.add(function (t) {display_cursor_time(t,cta);});
    var ctb = $("#cursor-time-b");
    cb.moved.add(function (t) {display_cursor_time(t,ctb);});

    var cursor_diff;
    function display_diff(t) {
      var diff = cb.getCursorTime() - ca.getCursorTime();
      $("#cursor-diff").each(function() {this.innerHTML = (diff/1e9)+"s";});
    }
    ca.moved.add(display_diff);
    cb.moved.add(display_diff);
    graph.setActiveCursor('a');
    graph.setSecondaryCursor('b');
    $("#graph").append(graph.getTop());
    <?php
    $signals = [];
    if ($_GET['signals']) {
      $signals = preg_split ("/,/", $_GET['signals']);
    } else if ($_COOKIE['signals']) {
      $signals = preg_split ("/,/", $_COOKIE['signals']);
    }
    foreach ($signals as $s)  {
      print "graph.addSignal(\"$s\");\n";
    }
    ?>
}
    </script>
    <title>Signaler (<?php echo $_GET['log']; ?>)</title>
    
  </head>
  <body onload="start();">
   <h1>Signaler (<?php echo $_GET['log']; ?>)</h1>
    <div id="graph"/>
   <table >
      <tr id="step_1ms" class="step">
	<td >
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.msec(-1);"/>
	</td>
	<td>1ms</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.msec(1);"/>
	</td>
      </tr>
      <tr id="step_10ms" class="step">
	<td >
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.msec(-10);"/>
	</td>
	<td>10ms</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.msec(10);"/>
	</td>
      </tr>
      <tr id="step_100ms" class="step">
	<td >
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.msec(-100);"/>
	</td>
	<td>100ms</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.msec(100);"/>
	</td>
      </tr>
      
      <tr id="step_1s" class="step">
	<td >
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.sec(-1);"/>
	</td>
	<td>1sek</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.sec(1);"/>
	</td>
      </tr>
      <tr id="step_10s" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.sec(-10);"/>
	</td>
	<td>10sek</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.sec(10);"/>
	</td>
      </tr>
      <tr id="step_1min" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.sec(-60);"/>
	</td>
	<td>1min</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.sec(60);"/>
	</td>
      </tr>
      <tr id="step_10min" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.sec(-600);"/>
	</td>
	<td>10min</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.sec(600);"/>
	</td>
      </tr>
      <tr id="step_1h" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.sec(-3600);"/>
	</td>
	<td>1tim</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.sec(3600);"/>
	</td>
      </tr>
      <tr id="step_1day" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.day(-1);"/>
	</td>
	<td>1dag</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.day(1);"/>
	</td>
      </tr>
      <tr id="step_1month" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.month(-1);"/>
	</td>
	<td>1mån</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.month(1);"/>
	</td>
      </tr>
      <tr id="step_1year" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.year(-1);"/>
	</td>
	<td>1år</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.year(1);"/>
	</td>
      </tr>
      <tr id="step_1page" class="step">
	<td>
	  <img src="adjust_left.svg" alt="Decrease" class="action" onclick="step.page(-1);"/>
	</td>
	<td>1sida</td>
	<td>
	  <img src="adjust_right.svg" alt="Increase" class="action" onclick="step.page(1);"/>
	</td>
      </tr>
      <tr id="zoom_in_out" class="zoom">
	<td  class="action" onclick="step.zoom(2);">-</td><td>Zoom</td>
	<td  class="action" onclick="step.zoom(.5);">+</td>
      </tr>
    </table>

    <?php
   try {
   $shown = "";
   $available = "";
   $dbh = new PDO('mysql:host=localhost;dbname=logging', 'logreader', null,
		  array(PDO::ATTR_PERSISTENT => true,
			PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION));
   $base_name = $_GET['log'];
   $res = $dbh->query("SELECT v.id,s.label,min(start), max(end), src, dest, bit_offset, bit_width FROM ".$base_name."_values as v, ".$base_name."_signals as s WHERE v.id = s.id GROUP BY id;");
   $res->bindColumn(1, $id);
   $res->bindColumn(2, $label);
   
   $res->bindColumn(3, $min_start);
   $res->bindColumn(4, $max_end);
   $res->bindColumn(5, $src);
   $res->bindColumn(6, $dest);
   $res->bindColumn(7, $offset);
   $res->bindColumn(8, $width);
   while($res->fetch(PDO::FETCH_BOUND)) {
     $min_time = (new DateTime("@".round($min_start/1e9)))->format("Y-m-d H:i:s");
     $max_time = (new DateTime("@".round($max_end/1e9)))->format("Y-m-d H:i:s");
     $line = "<tr id=\"signal_$id\">
<td  class=\"action\" onclick=\"move_signal_row($(this));\">$label</td>
<td class=\"action\" onclick=\"step.goTo($min_start);\">$min_time</td>
<td class=\"action\" onclick=\"step.goTo($max_end);\">$max_time</td>
<td>$src</td><td>$dest</td><td>$offset</td><td>$width</td>
</tr>\n";
     if (in_array($id, $signals)) {
       $shown .= $line;
     } else {
       $available .= $line;
     }
       
   }
 } catch (PDOException $e) {
   print "Error!: " . $e->getMessage() . "<br/>";
   die();
   }
$dbh = null;

?>
    <h2>Markörer</h2>
    Klicka i grafen för att placera A-markören, shift-klick för B.
    <ul>
      <li><span id="cursor-label-a">A</span>:
      <span id="cursor-time-a" class="action"
	    onclick="graph.moveToCursor('a');"/></li>
      <li><span id="cursor-label-b">B</span>:
      <span id="cursor-time-b" class="action"
	    onclick="graph.moveToCursor('b');"/></li>
      <li><span id="cursor-label-b">B</span>
      - <span id="cursor-label-a">A</span>: <span id="cursor-diff"/></li>
    </ul>
    <p><span title="Updatera grafen med valda signaler" class="action" onclick="update_signals(graph);">
      Updatera
    </span></p>
    <h2>Visas</h2>
    <table id="signals_shown">
      <tr>
	<th>Namn</th><th>Första</th><th>Sista</th>
	<th>Från</th><th>Till</th><th>Position</th><th>Bitar</th>
      </tr>
      <?php echo $shown ?>
    </table>
    <h2>Visas ej</h2>
    <table id="signals_available">
      <tr>
	<th>Namn</th><th>Första</th><th>Sista</th>
	<th>Från</th><th>Till</th><th>Position</th><th>Bitar</th>
      </tr>
      <?php echo $available ?>
    </table>
  </body>
</html>

