
<?php
  $SUCCESS              = "0";
  $DIRTY_ROTTEN_SPAMMER = "1";
  $DATABASE_UNAVAILABLE = "2";
  $DATABASE_ERROR       = "3";
  $EMAIL_UNAVAILABLE    = "4";
  $ERROR_QUERY_PARAMS   = "5";

  $TYPE_NEGATIVE_FEEDBACK = 1;
  $TYPE_POSITIVE_FEEDBACK = 2;
  $TYPE_NEUTRAL_FEEDBACK  = 3;
  $TYPE_BUG_REPORT        = 4;
  $TYPE_FEATURE_REQUEST   = 5;
  $TYPE_LICENSE_REQUEST   = 6;

  function get_host_name()
  {
      $ip = getenv("HTTP_CLIENT_IP");
      if (strlen($ip) == 0)
      {
          $ip = getenv("HTTP_X_FORWARDED_FOR");
          if (strlen($ip) == 0)
              $ip = getenv("REMOTE_ADDR");
      }
      if (strlen($ip))
          return gethostbyaddr($ip);
      return "unknown";
  }

  function sanitize_input($str)
  {
      if (isset($str))
          return strip_tags($str);
      return null;
  }

  function pdo_check_spam($db, $tablename, $host)
  {
      // Only allow known table names to prevent SQL injection via table name
      $allowed_tables = array('feedback', 'newsflash2');
      if (!in_array($tablename, $allowed_tables, true))
          return false;

      // check for flooding. if there are more than 2 messages from
      // the same host within the past 5 minutes posting is ignored
      $stmt = $db->prepare("SELECT id FROM $tablename WHERE UNIX_TIMESTAMP(date) >= UNIX_TIMESTAMP(NOW()) - 300 AND host LIKE :host");
      $stmt->execute(array(':host' => $host));
      $count = $stmt->rowCount();
      return ($count >= 2);
  }

?>
