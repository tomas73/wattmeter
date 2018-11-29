<html>
 <head>
  <title>Power Meter</title>
  <meta http-equiv="refresh" content="1">
 </head>
 <body>
 <?php 
	$output=shell_exec(power);
        echo $output.'Watt';
?> 
 </body>
</html>

