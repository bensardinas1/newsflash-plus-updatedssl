<?php
  include("common.php");
  include("database.php");
  include("credentials.php");

  $type     = sanitize_input($_REQUEST['type']);
  $name     = sanitize_input($_REQUEST['name']);
  $email    = sanitize_input($_REQUEST['email']);
  $country  = sanitize_input($_REQUEST['country']);
  $text     = sanitize_input($_REQUEST['text']);
  $version  = sanitize_input($_REQUEST['version']);
  $platform = sanitize_input($_REQUEST['platform']);
  $host     = sanitize_input(get_host_name());
  $tmpfile  = $_FILES['attachment']['tmp_name'];
  $filename = $_FILES['attachment']['name'];

  switch (intval($type)) {
      case $TYPE_NEGATIVE_FEEDBACK:
          $typename = "Feedback :(";
          break;
      case $TYPE_POSITIVE_FEEDBACK:
          $typename = "Feedback :)";
          break;
      case $TYPE_NEUTRAL_FEEDBACK:
          $typename = "Feedback :|";
          break;
      case $TYPE_BUG_REPORT:
          $typename = "Bug report";
          break;
      case $TYPE_FEATURE_REQUEST:
          $typename = "Feature request";
          break;
      case $TYPE_LICENSE_REQUEST:
          $typename = "License request";
          break;
  }

  if (!strlen($typename))
      die($ERROR_QUERY_PARAMS . " type");
  if (!strlen($name))
      die($ERROR_QUERY_PARAMS . " name");
  if(!strlen($text))
      die($ERROR_QUERY_PARAMS . " text");

  if (pdo_check_spam($db, "feedback", $host))
      die($DIRTY_ROTTEN_SPAMMER);

  $stmt = $db->prepare(
      "INSERT INTO feedback (type, name, email, host, country, text, version, platform) " .
      "VALUES (:type, :name, :email, :host, :country, :text, :version, :platform)"
  );
  $stmt->execute(array(
      ':type'     => $type,
      ':name'     => $name,
      ':email'    => $email,
      ':host'     => $host,
      ':country'  => $country,
      ':text'     => $text,
      ':version'  => $version,
      ':platform' => $platform,
  ));
  $id = $db->lastInsertId();

  // create and send email notification
  $subject = "[NEW FEEDBACK - ID: $id]";
  $text    = "Hello, new feedback was received\n\n" .
             "ID: $id\n" .
             "Type: $typename\n" .
             "Name: $name\n" .
             "Email: $email\n" .
             "Country: $country\n" .
             "Host: $host\n" .
             "Version: $version\n" .
             "Platform: $platform\n\n" .
             "$text";

  if (strlen($email))
      $headers = "From: $email\r\nReply-To: $email\r\n";

  if (strlen($tmpfile))
  {
      // Use finfo instead of shell_exec to avoid command injection
      $finfo    = new finfo(FILEINFO_MIME_TYPE);
      $mimetype = $finfo->file($tmpfile);
      $random_hash = bin2hex(random_bytes(16));
      $boundary    = "boundary-$random_hash";
      $safe_filename = htmlspecialchars(basename($filename), ENT_QUOTES, 'UTF-8');
      $attachment  = chunk_split(base64_encode(file_get_contents($tmpfile)));
      $body =  "This is a multi-part message in MIME format.\n\n" .
               "--$boundary\n" .
               "Content-Type: text/plain; charset=utf-8\n\n" .
               "$text\n" .
               "--$boundary\n" .
               "Content-Type: $mimetype; Name=\"$safe_filename\"\n" .
               "Content-Transfer-Encoding: base64\n" .
               "Content-Disposition: attachment; filename=\"$safe_filename\"\n\n" .
               "$attachment\n" .
               "--$boundary--" ;

      $headers .= "Content-Type: multipart/mixed; boundary=\"$boundary\"\n";
      $headers .= "Content-Transfer-Encoding: 7bit\n";
      $headers .= "MIME-Version: 1.0\n";
  }
  else
  {
      $headers .= "Content-Type: text/plain; charset=utf-8\r\n";
      $body = $text;
  }
  $mail_sent = @mail($MY_EMAIL, $subject, $body, $headers);
  if (!$mail_sent)
  {
      // todo: transactional semantics
  }
  echo $SUCCESS;

?>
