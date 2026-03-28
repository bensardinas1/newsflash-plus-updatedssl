<?php
  include("credentials.php");
  try {
      $db = new PDO("mysql:host=$DB_SERVER;dbname=$DB_NAME;charset=utf8mb4", $DB_USER, $DB_PASS);
      $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
      $db->setAttribute(PDO::ATTR_EMULATE_PREPARES, false);
  } catch (PDOException $e) {
      die($DATABASE_UNAVAILABLE);
  }
?>