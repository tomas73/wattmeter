<html>
<?php
	$power = shell_exec(power);

?>
  <head>
  <title>Power Meter</title>
  <meta http-equiv="refresh" content="1">
    <script type='text/javascript' src='https://www.google.com/jsapi'></script>
    <script type='text/javascript'>
      google.load('visualization', '1', {packages:['gauge']});
      google.setOnLoadCallback(drawChart);
      function drawChart() {
        var data = google.visualization.arrayToDataTable([
          ['Label', 'Value'],
          ['Watt', <?php echo $power; ?>]
        ]);

        var options = {
          width: 400, height: 400,
          max: 10000, min: 0,
          redFrom: 8000, redTo: 10000,
          yellowFrom:2000, yellowTo: 8000,
          greenFrom:0, greenTo: 2000,
          minorTicks: 5,
          majorTicks: ["0", "", "", "", "", "10000"]
        };

        var chart = new google.visualization.Gauge(document.getElementById('chart_div'));
        chart.draw(data, options);
      }
    </script>
  </head>
  <body>
    <div id='chart_div'></div>
  </body>
</html>
