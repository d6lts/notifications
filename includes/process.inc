<?php
// $Id$
/**
 * Notifications module. Queue processing.
 * 
 * Callbacks for queue processing. They may be implemented by other modules
 *    _load_user()
 *    _user_allowed()
 *    _process_send()
 * 
 * @ TO DO: Support different languages for message localization
 */

// Number of users to process for each step
define('NOTIFICATIONS_STEP_ROWS', 1000);
define('NOTIFICATIONS_STEP_USERS', 1000);

// Minimum amount of seconds the process will need for clean-up tasks
// Just to make sure that after exhausting cron assigned time we'll have a few spare seconds for some cleanup
define('NOTIFICATIONS_TIME_MARGIN', 5);

/**
 * Function to be called on cron by the main notifications_cron
 * 
 * It will call each subscription_process for each interval a number of times
 * 
 * This should send out messages starting with immediate delivery. We send first immediate delivery
 * because the other ones can be added up for each period. 
 * Assumption: The bigger the interval, the longer delay it may admit (?) I.e. sending hourly email 
 * after 1 hour 15 mins may be ok if the system is overloaded.
 */
function notifications_process_run($cron = TRUE) {  
  notifications_log('Starting notifications process');
  notifications_process('start');
  // There may be special time adjustments for cron
  if ($cron) {
    notifications_process('cron');
  }
  $stop = FALSE;
  $send_intervals = _notifications_send_intervals();
  unset($send_intervals[-1]);
  if ($max_sqid = notifications_process_prepare()) {
    foreach ($send_intervals as $interval => $name) {
      notifications_log('Processing queue', array('send interval' => $name));
      while (notifications_process_queue($interval, $max_sqid)) {
        $stop = !notifications_process('check');
      }
      if ($stop) {
        notifications_log('Process stopped, reached processing limits');
        break;
      } else {
        notifications_log('Process finished', array('send interval' => $name));
      }
    }
  } else {
    notifications_log('No rows in queue');
  }
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
  // Clean up expired logs from queue if logging enabled
  if ($keep = variable_get('notifications_log', 0)) {
    db_query("DELETE FROM {notifications_queue} WHERE cron = 0 AND sent < %d", time() - $keep);
  }
  // Clean up event table
  notifications_event_clean();
  // This will get the latest notification in queue so we don't mess with new ones being created during cron run
  // It will also prevent clashes with the immediate sending feature
  return db_result(db_query("SELECT max(sqid) FROM {notifications_queue}"));
}

/**
 * Clean up event table
 * 
 * @param $update
 *   Update event counter
 */
function notifications_event_clean($update = FALSE) {
  // This expiretime will prevent some race condition that occurs when the event is saved but the subs queue not yet populated  
  $expiretime = time() - 60; 
  if ($update) {
    // Update event counter, which keeps the number of notifications pending for each event
    db_query("UPDATE {notifications_event} e SET counter = (SELECT COUNT(*) FROM {notifications_queue} q WHERE q.eid = e.eid ) WHERE e.created < %d", $expiretime);
  }
  db_query("DELETE FROM {notifications_event} WHERE counter = 0 AND created < %d", $expiretime);
  // Delete events with no pending notifications. As events are created sequentially, we use this fact to speed up the query
  db_query("DELETE FROM {notifications_event} WHERE created < %d AND eid < (SELECT MIN(eid) FROM {notifications_queue})", $expiretime); 
}

/**
 * Controls and checks limits for queue processing
 * It can be used by other modules to add their own limits here, like number of sms sent, etc...
 * @param $op
 *   'start' => Start the counters
 *   'cron' => Special time adjustment for cron operations
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

  switch ($op) {
    case 'start':
      $defaults = variable_get('notifications_process_limit', array('time' => 0, 'message' => 0, 'row' => 0, 'percent' => 0));
      foreach ($defaults as $name => $value) {
        if ($value && !isset($limit[$name])) {
          $limit[$name] = $value;
        }
      }
      break;
    case 'cron':
      // Calculate time limit. We get the smaller of all these times in seconds
      // There's an issue with poormanscron not setting the cron semaphore so it will default to current time
      $timelimit = array();
      $cronstart = variable_get('cron_semaphore', time());     
      // Max execution time may be zero meaning no limit, then no limits based on this
      if ($maxtime = ini_get('max_execution_time')) {
        $timelimit[] =  $cronstart + $maxtime - NOTIFICATIONS_TIME_MARGIN;
        if (!empty($limit['percent'])) {
          $timelimit[] = time() + $maxtime * $limit['percent'] / 100;
          unset($limit['percent']);
        }
      }
      // This is an absolute limit, applies always if set
      if (!empty($limit['time'])) {
        $timelimit[] = time() + $limit['time'];
      }
      if ($timelimit) {
        $limit['time'] = min($timelimit);
      }
      break;
    case 'init':
      $current[$name] = 0;
      $limit[$name] = $value;
      break;

    case 'option':
      if (isset($value)) {
        $options[$name] = $value;
      }
      return isset($options[$name]) ? $options[$name] : FALSE;
      break;
    case 'limit':
      // Return limit value for counter
      return isset($limit[$name]) ? $limit[$name] : 0;
    case 'current':
      // Return current value for counter
      return isset($current[$name]) ? $current[$name] : 0;
    case 'count':
      $value =  $value ? $value : 1;
      isset($current[$name]) ? ($current[$name] += $value) : $current[$name] = $value;
      break;
    case 'check':
      // Check all limits till we find a false one
      $current['time'] = time();
      foreach ($limit as $name => $value) {
        if ($value && !empty($current[$name]) && $current[$name] >= $value) {
          watchdog('notifications', 'Reached processing limit on queue processing: %name = %value', array('%name' => $name, '%value' => $value));
          return FALSE;
        }
      }
      return TRUE;
  }

}

/**
 * Process rows given query conditions
 * 
 * This is the main notifications queue processing function, fetching queued notifications,
 * loading all related objects and sending out messages.
 * 
 * @see notifications_queue_query()
 * 
 * @param $conditions
 *   Array of query conditions
 * @param $limit
 *   Optional, limit the number of rows to process
 * @param $update
 *   Optional, update queue rows and event counter after processing
 * 
 * @return int
 *   Number of rows processed
 */
function notifications_process_rows($conditions, $limit = 0, $update = TRUE) {
  notifications_log('Processing queue rows', $conditions + array('limit' => $limit));
  $test = notifications_process('option', 'test');
  $count = 0;
  // Build query and fetch rows from queue
  $query = notifications_queue_query($conditions);
  $sql = "SELECT * FROM {notifications_queue}";
  $sql .= " WHERE ". implode(' AND ', $query['where']);
  $sql .= " ORDER BY module, mdid, send_interval";
  if ($limit) {
    $result = db_query_range($sql, $query['args'], 0, $limit);
  }
  else {
    $result = db_query($sql, $query['args']);
  }

  // Group rows by module, destination, send_interval before composing and sending
  // This loop has to run a final time after all rows have been fetched
  $last = $pending = NULL;
  while (($queue = db_fetch_object($result)) || $pending) {
    if (!$queue || $last && ($queue->module != $last->module || $queue->mdid != $last->mdid || $queue->send_interval != $last->send_interval)) {
      // New destination, send if not the first row and reset
      $count += notifications_process_queue_group($pending, $update, $last->module);
      $pending = NULL;
    }
    if ($queue) {
      notifications_process('count', 'row');
      // Add queue row to pending list
      $pending[$queue->mdid][$queue->send_interval][$queue->sqid] = $queue; 
    }
    $last = $queue;
  }
  // Done with queue, update event tracker
  if ($update) {
    notifications_event_tracker('update');
  }
  // Return number of rows processed
  return $count;
}

/**
 * Process queued rows, send messages, etc, etc...
 * 
 * @param $group
 *   Array of queue rows indexed by destination id, send interval, queue id
 */
function notifications_process_queue_group($group, $update, $module = 'notifications') {
  $subscriptions = $events = $processed = array();
  $test = notifications_process('option', 'test');
  $count = 0;
  foreach ($group as $mdid => $destination_group) {
    $destination = Messaging_Destination::load($mdid);
    $account = $destination ? $destination->get_account() : NULL;
    foreach ($destination_group as $send_interval => $interval_group) {
      foreach ($interval_group as $sqid => $queue) {
        $processed[] = $queue->sqid;
        $variables = array('queue sqid' => $queue->sqid , 'uid' => $queue->uid, 'event' => $queue->eid, 'destination' => $queue->mdid, 'send_interval' => $queue->send_interval);
        $count++;
        $event = notifications_event_tracker('load', $queue->eid);
        if (!$event) {
          notifications_log('Cannot load event', $variables);
        }
        elseif (!$destination) {
          notifications_log('Cannot load destination', $variables);
        }
        elseif (!$account) {
          notifications_log('Cannot load user for destination', $variables);
        }
        elseif (!notifications_event_user_access($event, $account)) {
          notifications_log('Access denied for event', $variables);
        }
        else {
          // This will take care of duplicated events
          $events[$queue->eid] = $event;
          // We keep track also of subscriptions originating this event
          $subscriptions[$queue->eid][] = $queue->sid;           
        }
      }  
    }
  }
  if ($events) {
    $messages = notifications_callback($module, 'process_compose', $destination, $events, $subscriptions, $send_interval);
    notifications_log('Composed messages', array('number' => count($messages), 'send_method' => $destination->method));
    // Note that we pass the testing parameter to notifications_process_send
    if ($messages) {
      notifications_callback($module, 'process_send', $destination, $messages, $test);
    }
    if (!$test) {          
      notifications_queue_update_sent($destination->mdid, $send_interval, time());
    }
  }
  if ($processed && $update) {
    notifications_queue_done(array('sqid' => $processed));        
  }
  return $count;
}

/**
 * Check user access to event's objects
 */
function notifications_event_user_access($event, $account) {
  $cache = &messaging_static(__FUNCTION__);
  if (!isset($cache[$event->eid][$account->uid])) {
    $cache[$event->eid][$account->uid] = notifications_user_allowed('event', $account, $event);
  }
  return $cache[$event->eid][$account->uid];
}

/**
 * Process subscriptions queue
 * 
 * The subscriptions queue has the following fields
 * sqid, uid, eid, sid, digest
 * 
 * This function should be able of splitting the whole processing in several steps.
 * It will be called multiple times for each send interval
 * 
 * Messages will be processed for each send interval, send_method, user
 * 
 * @param $send_interval
 *   Send interval to process
 * @param $max_sqid
 *   Max queue id to process
 * @param $language
 *   Optional language to process only rows in this language
 * 
 * @return Number of rows processed
 * 
 * @ TODO Review time conditions
 * @ TODO Per module queue processing
 */
function notifications_process_queue($send_interval, $max_sqid, $language = NULL) {
  notifications_log('Starting queue processing', array('send interval' => $send_interval, 'max squid' => $max_sqid));
  // Option for test running, marking messages as test, nor updating not sending
  $test = notifications_process('option', 'test');
  // Option for normal running but without updating the queue records
  $keep = notifications_process('option', 'keep');
  // Count processed rows
  $count = 0;
  // This is the time from which stored rows will be sent
  $timelimit = time() - $send_interval;
  // Check remaining rows to process to adjust query limits for both users and rows
  $step_users = NOTIFICATIONS_STEP_USERS;
  $step_rows = NOTIFICATIONS_STEP_ROWS;
  if ($row_limit = notifications_process('limit', 'row')) {
    $remaining_rows = $row_limit - notifications_process('current', 'row');
    if ($remaining_rows > 0) {
      $step_users = min($remaining_rows, $step_users);
      $step_rows = min($remaining_rows, $step_rows);
    }    
  }
  // Common batch parts for processing rows
  $default_batch = array(
    'cron' => 1,
    'max_sqid' => $max_sqid,
  );
  // Get users to process messages for, with this time interval and ordered by squid
  // Order by last sent for this send interval
  // Note: If we get the users with more messages pending first this may save some time
  $sql_select = "SELECT q.mdid, q.module, COUNT(q.sqid) AS count_rows FROM {notifications_queue} q ";
  $sql_select .= " LEFT JOIN {notifications_sent} su ON q.mdid = su.mdid AND q.send_interval = su.send_interval ";
  $sql_select .= " WHERE q.cron = 1 AND q.send_interval = '%d' AND q.sqid <= %d";
  $sql_select .= " AND (su.mdid IS NULL OR su.sent < %d) ";
  // Note: the group by su.sent seems to be needed by pgsql
  $sql_group = " GROUP BY q.mdid, q.module, su.sent ORDER BY su.sent";
  // If processing by language some things change
  if ($language) {
    $sql_select .= " AND q.language = '%s' ";
    $default_batch['language'] = $language->language;
    $result = db_query_range($sql_select . $sql_group, $send_interval, $max_sqid, $timelimit, $language->language, 0, $step_users);
  } else {
    $result = db_query_range($sql_select . $sql_group, $send_interval, $max_sqid, $timelimit, 0, $step_users);
  }
  // We create a bach for each mdid (user, destination, method) and handle it to notifications_process_rows()
  while (($queue = db_fetch_object($result)) && notifications_process('check')) {
    $module = $queue->module;
    $processed = array();    
    // Process all rows for this user. With some hard limit to prevent process lock ups.
    // In case we have too many rows, we go updating step by step
    if ($queue->count_rows > $step_rows) {
      $limit = $step_rows;
      // Still if we want to keep data, we don't update as we go
      $update = !$keep;
    }
    else {
      $limit = $queue->count_rows;
      $update = FALSE;
    }
    // Prepare batch query for actual row processing
    $batch = $default_batch + array(
      'mdid' => $queue->mdid, 'module' => $queue->module,
      'send_interval' => $send_interval,      
    );

    notifications_log('Queue processing', $batch);
    // These rows may be processed by a different module. Defaults to notifications_process_rows()
    $processed = notifications_callback($queue->module, 'process_rows', $batch, $limit, $update);
    $count += $processed;

    if ($processed && !$test && !$update && !$keep) {
      notifications_queue_done($batch);      
    }
  }

  // If not doing a test run, update event counter and return count
  // If doing a test run, return 0 so we don't go through this again
  if (!$test && !$keep) {
    notifications_event_tracker('update');
    return $count;
  }
  else {
    return 0;
  }
}

/**
 * Update user last time sent for each sending method / interval
 */
function notifications_queue_update_sent($mdid, $interval, $time) {
  db_query("UPDATE {notifications_sent} SET sent = %d WHERE mdid = %d AND send_interval = '%d'", $time, $mdid, $interval);
  if (!db_affected_rows()) {
    db_query("INSERT INTO {notifications_sent}(mdid, send_interval, sent) VALUES(%d, '%d', %d)", $mdid, $interval, $time);
  }
}

/**
 * Message composition.
 * 
 * Processes everything, included templating and digestion and sends message/s.
 * 
 * Adds some more information into $message['notifications'] that may be used by other modules
 *
 * @param $account
 *   User account to send the notification to
 * @param $events
 *   Array of loaded event objects to be processed
 * @param $subscriptions
 *   Array of arrays of subscription ids (sids) for each event(eid)
 * 
 * @return array()
 *   Array of messages ready for sending out
 */
function notifications_process_compose($destination, $events, $subscriptions, $send_interval, $module = 'notifications') {
  notifications_log('Processing for sending', array('method' => $destination->method, 'interval' => $send_interval, 'module' => $module, 'events' => count($events)));
  // Find build method for this interval. Check the function is there in case some module has been disabld
  if ($build_method = notifications_process_build_method($send_interval)) {
    $build_function = $build_method['build callback'];
  }
  else {
    // Default building function
    $build_function = 'notifications_process_build_simple';
  }
  // Invoke building function that will return an array of messages  
  return $build_function($destination, $events, $subscriptions, $send_interval, $module);
}

/**
 * Information about digesting method for a send interval.
 * 
 * @return array()
 *   Ditest information for that interval, or all the information if no interval
 */
function notifications_process_build_method($send_interval = NULL, $refresh = FALSE) {
  static $intervals;
  $build_methods = &messaging_static(__FUNCTION__);
  if (!isset($build_methods) || $refresh) {
    // Method information
    foreach (notifications_module_information('build methods') as $method) {
      $build_methods[$method['type']] = $method;
    }
    // Mapping interval -> method
    $intervals = variable_get('notifications_digest_methods', array());
  }
  if (is_null($send_interval)) {
    return $build_methods;
  }
  elseif (!empty($intervals[$send_interval]) && isset($build_methods[$intervals[$send_interval]])) {
    return $build_methods[$intervals[$send_interval]];
  }
  else {
    // Default, that will be always the simple one
    return $build_methods['simple'];
  }
}

/**
 * Send array of messages through messaging module
 * 
 * @param $destination
 *   Messaging destination to send to
 * @param $messages
 *   Array of messages prepared for sending
 * @param $test
 *   Optional just test composition and formating but do not send
 */
function notifications_process_send($destination, $messages, $test = FALSE) {  
  foreach ($messages as $message) {
    notifications_process('count', 'send');
    notifications_debug('Sending out notification', array('method' => $destination->method, 'message' => $message));
    notifications_message_send($destination, $message, $test);
  }
  return $messages;
}

/**** Retrieving and replacing text parts, interfacing with tokens and messaging module ****/

/**
 * Get message part
 * 
 * It searches for optional message group keys for options defaulting to $type
 * 1. $module-$type-[$event->type]-[$event->action]
 * 2. $module-$type-[$event->type]
 * 3. $module-$type
 * 
 * @param $type
 *   Message type to send, either 'event' or 'digest'
 * @param $key
 *   Id of message part, ie 'header'
 * @param $method
 *   Method by which message will be sent. Normally 'mail'
 * @param $param
 *   Event data if we have a single event (type = event), none if we are digesting multiple events (type = digest)
 * @param $module
 *   Module name to be prefixed to the template name. If different than notifications we first try
 *   with that module but if not found, try again with 'notifications'
 * 
 * @return
 *   Part of the message with tokens for replacement.
 */
function notifications_event_message_part($event, $key, $method, $module = 'notifications') {
  // If event passed check for predefined text or get optional keys from event
  if ($text = $event->get_text($key)) {
    return $text;
  }
  elseif ($template = notifications_event_get_template($event)) {
    $text = messaging_template_text_part($template, $key, $method);
    if (isset($text)) {   
      return $text;
    }
  }
  // Default, template not found
  return "[UNDEFINED module = $module, key = $key, method = $method, event = $event->eid, type = $event->typekey]";
}


/**
 * Message sending, pass the message to Messaging back end
 * 
 * @param $account
 *   User account to send the message to
 * @param $message
 *   Message array, will be converted to object
 * @param $send_method
 *   Send method
 * @param $test
 *   Optional, set to TRUE if doing a test run (messages not to be actually sent)
 * 
 * @return boolean
 *   TRUE if sending was successfull
 */
function notifications_message_send($destination, $message, $test = FALSE) {
  notifications_debug('Preparing user notification for messaging', array('message' => $message, 'destination' => $destination));
  $message = (object)$message;
  $message->type = 'notifications';
  $message->test = $test;
  notifications_process('count', 'message');
  messaging_message_send_destination($destination, $message);
}

/**
 * Get events with static caching. Handle event deletion if not available anymore
 */
function notifications_event_load($id) {
  $cached = Notifications_Event::cache_get($id);
  if (isset($cached)) {
    return $cached;
  }
  elseif ($event = Notifications_Event::load($id)) {
    // Load aditional objects for the event. For backwards compatibility
    // For modules up to date events are loaded with $event->load_objects()
    notifications_module_invoke('event load', $event);
    return $event;
  }
  else {
    Notifications_Event::cache_set($id, FALSE);
  }
}

/**
 * Keep track of events and update event counter with processed rows eids
 * 
 * It will delete events when objects are not available anymore
 * 
 * @param $op
 *   load, count, reset, update
 * @param $event
 *   event object to track
 */
function notifications_event_tracker($op, $event = NULL) {
  $tracker = &messaging_static(__FUNCTION__);
  //notifications_debug('Event tracker', array('op' => $op, 'event' => $event));
  switch ($op) {
    case 'load': // Load and add to the tracker, $event is eid
      if (($event = notifications_event_load($event)) && $event->load_objects() && empty($event->delete)) {
        $event->track_count();
        $tracker[$event->eid] = $event;
        return $event;
      }
      break;
    case 'count':
      $event->track_count();
      break;
    case 'delete':
      // Delete event and all related rows.
      unset($tracker[$event->eid]);
      $event->delete();
      break;
    case 'update':
      // Update tracked events counter or delete if counter reached zero or marked for deletion.
      if ($tracker) {
        foreach ($tracker as $event) {
          if (!$event->counter || $event->incomplete || !empty($event->delete)) {
            $event->delete();
          }
          else {
            $event->update_counter();
          }
        }
      }
      // Intentional no break (update will also reset)
    case 'reset':
      $tracker = array();
  }  
}

/**
 * Mark queue rows as done
 * 
 * Either log, if logging enabled, or delete
 */
function notifications_queue_done($params) {
  if (variable_get('notifications_log', 0)) {
    notifications_queue_update($params, array('cron' => 0, 'sent' => time()));
  } else {
    notifications_queue_delete($params);
  }
}

/**
 * Update queue rows with defined values
 * 
 * @arg $params
 *   Parameters to select the queue rows for updating. Array of field => value pairs
 * @arg $update
 *   Fields values to update. Array of field => value pairs
 */
function notifications_queue_update($params, $updates) {
  $values =  _messaging_query_conditions('notifications_queue', $updates);
  $where = notifications_queue_query($params);
  $args = array_merge($values['args'], $where['args']);
  return db_query('UPDATE {notifications_queue} SET ' . implode(', ', $values['conditions']) . ' WHERE ' . implode(' AND ', $where['where']), $args);
}

/**
 * Delete rows from subscriptions queue
 * 
 * @see notifications_queue_query()
 * 
 * Note: Handle with care if no params may delete all rows
 */
function notifications_queue_delete($params) {
  $query = notifications_queue_query($params);
  db_query("DELETE FROM {notifications_queue} WHERE ". implode(' AND ', $query['where']), $query['args']);
}

/**
 * Build query conditions for queue queries
 * 
 * @param $params
 *   Array of parameters, field => value form
 *   Special parameters
 *     'max_squid' => max squid to delete
 *     'rows' => array of squid values to delte
 * @return
 *   Array with 'where' and 'args' elements. Each of them is an array
 */
function notifications_queue_query($params, $table_alias = NULL) {
  $where = $args = array();
  
  // Special condition max_sqid
  if (isset($params['max_sqid'])) {
    $where[] = $table_alias ? "$table_alias.sqid <= %d" : "sqid <= %d";
    $args[] = $params['max_sqid'];
    unset ($params['max_sqid']);
  }
  // User generic query builder for the rest of fields
  $values =  _messaging_query_conditions('notifications_queue', $params, $table_alias);  
  $where = array_merge($where, $values['conditions']);
  $args = array_merge($args, $values['args']);
 
  return array('where' => $where, 'args' => $args);
}

/**
 * Digest multiple events in a single message, short format.
 * 
 * @return array with messages ready to be sent
 */
function notifications_process_build_simple($destination, $events, $subscriptions, $send_interval, $module = 'notifications') {
  $messages = array();
  $sender_option = variable_get('notifications_sender', 0);
  foreach ($events as $event) {
    $event_subscriptions = isset($subscriptions[$event->eid]) ? $subscriptions[$event->eid] : NULL;
    $message = notifications_process_build_simple_message($destination, $event, $event_subscriptions);
    // We pass on the full information so it can be used by modules implementing some of the hooks
    $message['notifications'] = array('events' => array($event), 'subscriptions' => $event_subscriptions);
    // Optional sender, if chosen will be the user account who produced the event
    // It will be up to the sending method modules what to do with this information.
    if ($sender_option) {
      $sender = notifications_load_user($event->uid);
      $message['sender_name'] = $sender->name;
      if ($sender_option == 2) {
        $message['sender_account'] = $sender;
      }
    }
    $messages[] = $message;
  }
  return $messages;
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
 * @return
 *   Message array 
 */
function notifications_process_build_simple_message($destination, $event, $subscriptions) {
  $send_method = $destination->method;
  $account = $destination->get_account();
  // Create message. Do all this in one replacemente
  $text = array(
    'subject' => notifications_event_message_part($event, 'subject', $send_method),
    'header' => notifications_event_message_part($event , 'header', $send_method),
    'event'  => notifications_event_message_part($event, 'main', $send_method),
    'footer' => notifications_event_message_part($event, 'footer', $send_method),
  );
  // We pass only the first subscription, which is at least something
  // @ TODO Handle nicely the case where there are more than one subscription
  if ($sid = array_shift($subscriptions)) {
    $subscription = notifications_load_subscription($sid);
  }
  else {
    $subscription = NULL;
  }
  $objects = array('destination' => $destination, 'user' => $account, 'event' => $event, 'subscription' => $subscription);
  $objects = array_merge($objects, $event->get_objects());
  $text = messaging_template_text_replace($text, $objects);

  // Get subject out of text and build the message array
  $subject = $text['subject'];
  unset($text['subject']);
  return array('subject' => $subject, 'body' => $text);
}
