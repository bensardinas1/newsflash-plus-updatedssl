
<?php
  // this interface PHP script is used by Newsflash Plus 4.0.0 and newer.
  // prior versions have an older interface in newsflash.php

  include("common.php");
  include("database.php");
  include("version.php");

  $platform    = sanitize_input($_REQUEST['platform']);
  $version     = sanitize_input($_REQUEST['version']);
  $fingerprint = sanitize_input($_REQUEST['fingerprint']);
  $host        = sanitize_input(get_host_name());

  $stmt = $db->prepare(
      "INSERT INTO newsflash2 (fingerprint, host, version, platform, count) " .
      "VALUES(:fingerprint, :host, :version, :platform, 1) " .
      "ON DUPLICATE KEY UPDATE latest=now(), count=count+1, host=:host2, version=:version2, platform=:platform2"
  );
  $stmt->execute(array(
      ':fingerprint' => $fingerprint,
      ':host'        => $host,
      ':version'     => $version,
      ':platform'    => $platform,
      ':host2'       => $host,
      ':version2'    => $version,
      ':platform2'   => $platform,
  ));

  //echo($NEWSFLASH_VERSION);
  echo($NEWSFLASH_VERSION_DEV);

  //echo("\r\n");
  //echo($count);
?>
