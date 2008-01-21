<?php
/**
 * Subscriptions module. Queue processing.
 * 
 * @ TO DO: Support different languages for message localization
 */

// Number of users to process for each step
define('NOTIFICATIONS_STEP_ROWS', 100);
define('NOTIFICATIONS_STEP_USERS', 100);

/**
 * Function to be called on cron by the main notifications_cron
 * 
 * It will call each subscription_process for each interval a number of times
 * 
 * This should send out messages starting with inmediate delivery. We send first inmediate delivery
 * because the other ones can be added up for each period. 
 * Assumption: The bigger the interval, the longer delay it may admit (?) I.e. sending hourly email 
 * after 1 hour 15 mins if the system is overloaded.
 */
function notifications_process_run() {  
  $output[] = 'Starting process';
  notifications_process('start');

  $stop = FALSE;
  $send_intervals = _notifications_send_intervals();
  unset($send_intervals[-1]);
  $max_sqid = notifications_process_prepare();
  foreach ($send_intervals as $interval => $name) {
    notifications_log('Processing send interval '.$name);
    while (notifications_process_queue($interval, $max_sqid)) {
      $stop = !notifications_process('count', 'step');
    }
    if ($stop) {
      notifications_log('Process stopped');
      break;
    }
  }
  return $output;
}

/**
 * Support of pull messaging
 * 
 * @see messaging_pull_pending()
 * 
 * @return
 *  Array of pending messages
 */
function notifications_process_pull($method, $users, $limit, $delete) {
  $messages = array();
  $maxsqid = 0;
  // Just fetches row in creation order de-duping same events on the step
  $sql = "SELECT uid, eid, MIN(sid) AS sid, MAX(sqid) AS sqid FROM {notifications_queue} ";
  $sql .= "WHERE send_method = '%s' AND uid IN (%s) GROUP BY uid, eid ORDER BY sqid";
  $str_uids = implode(',', $users);
  if ($limit) {
    $result = db_query_range($sql, $method, $str_uids, 0, $limit);
  } else {
    $result = db_query($sql, $method, $str_uids);
  }
  
  // Fetch and prepare messages
  while ($queue = db_fetch_object($result)) {
    $maxsqid = $queue->squid;
    $account = notifications_load_user($queue->uid);
    $event = notifications_load_event($queue->eid);
    $subscriptions = array($queue->sid);
    $message = notifications_process_message($account, $event, $subscriptions, $method);
    $message['uid'] = $queue->uid;
    $message['from'] = $event->uid;
    $messages[] = $message;   
  }
  // Delete returned rows
  if ($messages && $delete) {
    db_query("DELETE FROM {notifications_queue} WHERE sqid < %d AND send_method = '%s' AND uid IN (%s)", $maxsqid, $method, $str_uids);
  }

  // Return collected messages;
  return $messages;
}

/**
 * Prepare subscriptions queue
 * 
 * This is intended to avoid race conditions where new rows are added while the process is running
 * 
 * @return
 *   Max $sqid that will be processed this cron
 */
function notifications_process_prepare() {
  // Another funny race condition may occur when the event is saved but the subs queue not yet populated  
  $expiretime = time() - 60;
  // Clean up event table.
  // @ TODO Consider a different timing for this cleanup
  db_query("DELETE FROM {notifications_event} WHERE created < %d AND eid NOT IN (SELECT eid FROM {notifications_queue})", $expiretime);
  $squid = db_result(db_query("SELECT max(sqid) FROM {notifications_queue}"));
  if ($squid) {
    // Delete non existent or blocked users
    db_query("DELETE FROM {notifications_queue} WHERE uid NOT IN (SELECT uid FROM {users} WHERE status = 1)");
  }
  // @ TODO We may use this step to fill in some data or doing some other optimizations
  return $squid;
}

/**
 * Controls limits for queue processing
 * It can be used by other modules to add their own limits here, like number of sms sent, etc...
 * @param $op
 *   'start' => Start the counters
 *   'init' => Start a new counter with $value limit
 *   'option' => Sets /gets options
 *      - debug
 *      - output Enables output for admin page
 * @return
 *   TRUE if we are yet under the processing limits
 */
function notifications_process($op = 'check', $name = NULL, $value = NULL) {
  static $limit = array(), $options = array();
  static $current = array('message' => 0, 'step' => 0);

  switch($op) {
    case 'start':
      $defaults = variable_get('notifications_process_limit', array('time' => 0, 'message' => 0, 'row' => 0, 'step' => 0));
      foreach($defaults as $name => $value) {
        if ($value && !isset($limit[$name])) {
          $limit[$name] = $value;
        }
      }
      if (isset($limit['time'])) {
        $limit['time'] += time();
      }     
      break;
    case 'init':
      $current[$name] = 0;
      $limit[$name] = $value;
      break;
    case 'count':
      $current[$name] += $value ? $value : 1;
      break;
    case 'option':
      if (isset($value)) {
        $options[$name] = $value;
      }
      return $options[$name];
  }
  $current['time'] = time();
  
  // Check all limits till we find a false one
  foreach ($limit as $name => $value) {
    if (($current[$name] >= $value)) {
      watchdog('notifications', t('Reached processing limit on queue processing: %name = %value', array('%name' => $name, '%value' => $value)));
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Process subscriptions queue
 * 
 * The subscriptions queue has the following fields
 * sqid, uid, eid, sid, digest
 * 
 * This function should be able of splitting the whole processing in several steps.
 * It will be called multiple time for each send interval
 * 
 * Messages will be processed for each send interval, send_method, user
 * 
 * @return Number of rows processed
 * 
 * @ TODO Review time conditions
 */
function notifications_process_queue($send_interval, $max_sqid) {
  $count = 0;
  $digest = $send_interval > 0;
  // This is the time from which stored rows will be sent
  $timelimit = time() - $send_interval;
  // Get users to process messages for, with this time interval and ordered by squid
  // Order by last sent for this send interval
  // Note: If we get the users with more messages pending first this may save some time
  $sql = "SELECT q.uid, q.send_method, count(*) AS count FROM {notifications_queue} q ";
  $sql .= " LEFT JOIN {notifications_sent} su ON q.uid = su.uid AND q.send_interval = su.send_interval AND q.send_method = su.send_method ";
  $sql .= " WHERE q.cron = 1 AND q.send_interval = '%d' AND q.sqid <= %d ";
  $sql .= " AND (su.uid IS NULL OR su.sent < %d) ";
  $sql .= " GROUP BY q.uid, q.send_method ORDER BY su.sent";
  $result = db_query_range($sql, $send_interval, $max_sqid, $timelimit, 0, NOTIFICATIONS_STEP_USERS);

  $account = NULL;
  $current_uid = 0;
  // @ TODO Add time conditions
  while (($user = db_fetch_object($result)) && notifications_process('check')) {
    notifications_log("Processing user $user->uid, rows $user->count, send_method $user->send_method");
    $events = $subscriptions = array();    
    $send_method = $user->send_method;
    $sqid = 0;
    $account = notifications_load_user($user->uid);
    // Process all rows for this user. With some hard limit to prevent process lock ups.
    $result_subs = db_query_range("SELECT * FROM {notifications_queue} WHERE send_interval = '%d' AND uid = %d AND sqid <= %d ORDER BY send_method, sqid", $send_interval, $account->uid, $max_sqid, 0, NOTIFICATIONS_STEP_ROWS);
    while (($queue = db_fetch_object($result_subs)) && notifications_process('count', 'row')) {
      $count++;
      $sqid = $queue->sqid;
      // This will take care of duplicated events
      $events[$queue->eid] = $event = notifications_load_event($queue->eid, TRUE);
      // We keep track also of subscriptions originating this event
      $subscriptions[$queue->eid][] = $queue->sid;
      notifications_log("Processing queued sid=$queue->sid event=$queue->eid ($event->type, $event->action) send_method=$send_method");

    }
    if ($events) {
      notifications_process_send($account, $events, $subscriptions, $send_method, $send_interval);
      if (!notifications_process('option', 'debug')) {
        notifications_queue_delete(array('uid' => $user->uid, 'send_interval' => $send_interval, 'send_method' => $send_method, 'max_sqid' => $sqid));
        notifications_update_sent($user->uid, $send_method, $send_interval, time());
      }      
    }
  }
  
  return $count;
}

/**
 * Update user last time sent
 */
function notifications_update_sent($uid, $method, $interval, $time) {
  db_query("UPDATE {notifications_sent} SET sent = %d WHERE uid = %d AND send_interval = '%d' AND send_method = '%s'", $time, $uid, $interval, $method);
  if (!db_affected_rows()) {
    db_query("INSERT INTO {notifications_sent}(uid, send_interval, send_method, sent) VALUES(%d, '%d', '%s', %d)", $uid, $interval, $method, $time);
  }
}
/**
 * Message delivery.
 * 
 * Processes everything, included digestion and sends message/s.
 */
function notifications_process_send($account, $events, $subscriptions, $send_method, $send_interval) {

  notifications_log("Sending out, method=$send_method, interval=$send_interval, events=". count($events));
  // Digest if send_interval > 0 (not inmediate sending)
  if ($send_interval) {
    $messages[] = notifications_process_digest($account, $events, $send_interval, $send_method);
  } else {
    foreach ($events as $event) {
      $messages[] = notifications_process_message($account, $event, $subscriptions[$event->eid], $send_method);
    }
  }
  // Now send messages
  foreach ($messages as $message) {
    notifications_process('count', 'send');
    // notifications_log($message, 'message');
    notifications_message_send($account, $message, $send_method);
  }
}

/**
 * Digest multiple events in a single message
 * 
 * @return array with message 'subject' and 'body' elements
 */
function notifications_process_digest($account, $events, $send_interval, $send_method) {
  // Compile list of events for each object
  $list = array();

  // Build up the digested list with text replacement
  // We need text replacement for each line because it depends on different objects
  foreach ($events as $event) {
    notifications_log($event, 'digesting');
    $info = notifications_event_types($event->type, $event->action);
    $digest_type = $info['digest'][0];
    $digest_field = $info['digest'][1];
    $digest_object = $event->objects[$digest_type];
    $digest_value = $digest_object->$digest_field;
    if (!isset($list[$digest_type][$digest_value]['name'])) {
      $name = $list[$digest_type][$digest_value]['name'] = notifications_text_replace($info['name'], array($digest_type => $digest_object));
      notifications_log("Digesting object $name (type=$digest_type value=$digest_value)");
    }
    // Check duplicate notifications for the same event
    if (!isset($list[$digest_type][$digest_value]['line'][$event->eid])) {
      $line = $list[$digest_type][$digest_value]['line'][$event->eid] = notifications_text_replace($info['line'], $event->objects);
      notifications_log("Digesting line $line");
    }
  }
  // Create message. Do all this in one replacement, then strip out the subject
  $text['subject'] = notifications_message_part('digest', 'subject', $send_method);
  $text['header'] = notifications_message_part('digest', 'header', $send_method);
  $text['footer'] = notifications_message_part('digest', 'footer', $send_method);
  // We dont pass a subscription object here, won't be too much use anyway
  $text = notifications_text_replace($text, array('user' => $account, 'subscription' => NULL));
  
  // Compose body. All these lines have been text replaced
  $body['header'] = $text['header'];
  foreach ($list as $type => $objects) {
    foreach ($objects as $oid => $data) {
      $body[] = $data['name'];
      foreach ($data['line'] as $line) {
        $body[] = '- '. $line;
      }
    }
  }
  $body['footer'] = $text['footer'];
  
  return array('subject' => $text['subject'], 'body' => $body);
}

/**
 * Creates a single message for a single event
 * 
 * @param $account
 *   Destination user account
 * @param $event
 *   Event object which caused this notification
 * @param $subscriptions
 *   Array of subscription ids
 * 
 */
function notifications_process_message($account, $event, $subscriptions, $send_method) {
  $info = notifications_event_text($event);

  // Create message. Do all this in one replacemente
  $options = array($event->type, $event->action);
  $text = array(
    'subject' => $info['title'],
    'header' => notifications_message_part('event' ,'header', $send_method, $options),
    'event'  =>  $info['detail'],
    'footer' => notifications_message_part('event', 'footer', $send_method, $options),
  );
  // We pass only the first subscription, which is at least something
  // @ TODO Handle nicely the case where there are more than one subscription
  if ($sid = array_shift($subscriptions)) {
    $subscription = notifications_load_subscription($sid);
  } else {
    $subscription = NULL;
  }
  $objects = array('user' => $account, 'event' => $event, 'subscription' => $subscription);
  $objects = array_merge($objects, $event->objects);
  $text = notifications_text_replace($text, $objects);

  // Get subject out of text
  $subject = $text['subject'];
  unset($text['subject']);
  return array('subject' => $subject, 'body' => $text);
}

/**** Retrieving and replacing text parts, interfacing with tokens and messaging module ****/

/**
 * Get message part
 * 
 * It searches for optional message keys for options defaulting to $key
 * thus if $options = array('part1', 'part2') it will search in this order
 * 1. $key-part1-part2
 * 2. $key-part1
 * 3. $key
 * 
 * @param $type
 *   Message type to send, either 'event' or 'digest'
 * @param $key
 *   Id of message part, ie 'header'
 * @param $method
 *   Method by which message will be sent. Normally 'mail'
 * @param $options
 *   ?
 *
 * @return
 *   Part of the message.
 *
 */
function notifications_message_part($type, $key, $method, $options = array()) {
  $keyparts = array_merge(array($key), $options);
  // Output some debugging info in case we dont find a suitable message part
  $output = "[UNDEFINED type = $type, method = $method, key = ".implode('-', $keyparts).']';
  while ($keyparts) {
    $msgkey = implode('-', $keyparts);
    if ($text = messaging_message_part("notifications-$type", $msgkey, $method)) {
      $output = $text;
      break;
    }
    // If no text trim out latest part of the key and retry
    array_pop($keyparts);
  }
  return $output;
}

/**
 * Text replacement with tokens
 */
function notifications_text_replace($text, $objects) {
  // Add some token types
  $objects['global'] = NULL;  
  return token_replace_multiple($text, $objects);
}

/**
 * Message sending
 */
function notifications_message_send($account, $message, $send_method) {
  $message['type'] = 'notifications';
  notifications_process('count', 'message');
  messaging_message_send_user($account, $message, $send_method);
}

/**
 * Get texts for event
 * 
 * @ TODO Support for configurable texts
 */
function notifications_event_text($event) {
  $info = notifications_event_types($event->type, $event->action);
  return $info;
}

/**
 * Get users with static caching
 */
function notifications_load_user($uid) {
  static $cache = array();
  if (!array_key_exists($uid, $cache)) {
    $cache[$uid] = user_load(array('uid' => $uid));
  }
  return $cache[$uid];
}

/**
 * Get events with static caching
 */
function notifications_load_event($id) {
  static $cache = array();
  if (!array_key_exists($id, $cache)) {
    $event = db_fetch_object(db_query("SELECT * FROM {notifications_event} WHERE eid = %d", $id));
    $event->params = unserialize($event->params);
    // Load aditional objects for the event
    $event->objects = array();
    notifications_module_invoke('event load', $event);
    $cache[$id] = $event;
  }
  return $cache[$id];
}

/**
 * Delete rows from subscriptions queue
 * 
 * Note: Handle with care if no params may delete all rows
 */
function notifications_queue_delete($params) {
  $where = $args = array();
  if (isset($params['max_sqid'])) {
    $where[] = "sqid <= %d";
    $args[] = $params['max_sqid'];
    unset($params['max_sqid']);
  }
  foreach($params as $field => $value) {
    $where[] = "$field = '%s'";
    $args[] = $value;
  }
  db_query("DELETE FROM {notifications_queue} WHERE ".implode(' AND ', $where), $args);
}

/**
 * Log for debugging
 */
function notifications_log($info = NULL, $type = 'info') {
  static $logs;
  if ($info) {
    $message = $type.': ';
    $message .= is_string($info) ? $info : print_r($info, TRUE);
    $logs[] = $message;
    if ($type == 'watchdog') {
      watchdog('notifications', $info);
    }
  } else {
    return $logs;
  }
}