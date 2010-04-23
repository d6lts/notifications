<?php
// $Id$
/**
 * @file
 * Notifications event library
 * 
 * We just need this on pages where events are triggered or queue processing is done
 */

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
 * Get template for event
 * 
 * @param $event
 *   It may be event object or event type key
 */
function notifications_event_get_template($event) {
  $info = &messaging_static(__FUNCTION__);
  if (!isset($info)) {
    $info = variable_get('notifications_event_template', array());
  }
  if (is_object($event)) {
    // An event object will know about its template itself
    return $event->get_template();
  }  
  else {
    // $event is just the type key
    if (!isset($info[$event])) {
      if ($template = notifications_event_types($event, 'template')) {
        // Use default value from event definition
        $info[$event] = $template;
      }
      else {
        $info[$event] = FALSE;
      }
    }
    return $info[$event];
  }
}

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
 * @param $debug
 *   Produce debug text with information about templates.
 *   
 * @return
 *   Part of the message with tokens for replacement.
 */
function notifications_event_message_part($event, $key, $method, $language, $module = 'notifications') {
  // If event passed check for predefined text or get optional keys from event
  if ($text = $event->get_text($key)) {
    return $text;
  }
  elseif ($template = notifications_event_get_template($event)) {
    $text = messaging_template_text_part($template, $key, $method, $language);
  }
  // Default, template not found, return some information if debugging
  return isset($text) ? $text : '';
}

/**
 * Store / return events for immediate sending
 */
function notifications_event_send_immediate($event = NULL) {
  $events = &messaging_static(__FUNCTION__);  
  if ($event) {
    // Store events anyway so they can be accessed through the static variable
    // and other modules can have a peek on what has happened on this request
    $events[$event->eid] = $event;
  }
  elseif (variable_get('notifications_send_immediate', 0)) {
    return $events;
  }
}

/**
 * Process events stored for immediate sending
 * 
 * This will be called from notifications_exit() when the event API is loaded
 */
function notifications_event_process_immediate() {
  if ($events = notifications_event_send_immediate()) {
    notifications_include('process.inc');
    foreach ($events as $event) {
      notifications_process_rows(array('cron' => 1, 'eid' => $event->eid, 'send_interval' => 0));
    }    
  }
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
    // Inform other modules, give them the chance to alter
    module_invoke_all('notifications_event', 'load', $event);
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
 * Trigger event. Save, run queue query, etc...
 */
function notifications_event_trigger($event) {
  // Notify other modules we are about to trigger some subscriptions event
  // Modules can do cleanup operations or modify event properties
  module_invoke_all('notifications_event', 'trigger', $event);
  
  // Store event, unles marked not to be saved 
  if ($event->save) {
    $event->save();
    // Send event to queue for subscriptions, unless marked not to
    if ($event->queue) {
      $event->queued = TRUE;
      notifications_include('query.inc');
      $count = notifications_query_queue_event($event);
      // Now update event counter with rows in notifications_queue or delete if no rows
      if ($count) {
        $event->update_counter($count);
        // If immediate sending enabled, store it for sending on page exit.
        notifications_event_send_immediate($event);
      }
      else {
        $event->delete();
      }
    }
  }
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
