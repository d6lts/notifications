<?php
// $Id$
/**
 * @file
 * 
 * Notifications module. Queue processing API.
 * 
 */

/**
 * Information about digesting method for a send interval.
 * 
 * @return array()
 *   Ditest information for that interval, or all the information if no interval
 */
function notifications_process_build_method($send_interval = NULL, $refresh = FALSE) {
  $build_methods = notifications_info('build methods', NULL, $refresh);
  $intervals = variable_get('notifications_digest_methods', array());

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
 * Digest multiple events in a single message, short format.
 * 
 * @return array with messages ready to be sent
 */
function notifications_process_build_simple($template, $events, $subscriptions, $module = 'notifications') {
  $messages = array();
  $sender_option = variable_get('notifications_sender', 0);
  foreach ($events as $event) {
    $event_subscriptions = isset($subscriptions[$event->eid]) ? array_filter($subscriptions[$event->eid]) : NULL;
    $message = notifications_callback($module, 'process_build_simple_message', $template, $event, $event_subscriptions, $module);
    $message->set_sender($event->uid);
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
 * @param $debug
 *   Return template parts information with the message
 * 
 * @return
 *   Message object
 */
function notifications_process_build_simple_message($template, $event, $subscriptions, $module = 'notifications') {
  $send_method = $template->method;
  $destination = $template->get_destination();
  $account = $template->get_user();
  $language = $template->get_language();

  // Start the message by cloning the template
  $message = clone $template;
  $message->add_event($event, $subscriptions);
  // Create message. Do all this in one replacemente
  $parts = array(
    'subject' => notifications_event_message_part($event, 'subject', $send_method, $language, $module),
    'header' => notifications_event_message_part($event , 'header', $send_method, $language, $module),
    'event'  => notifications_event_message_part($event, 'main', $send_method, $language, $module),
    'footer' => notifications_event_message_part($event, 'footer', $send_method, $language, $module),
  );
  // We pass only the first subscription, which is at least something
  // @ TODO Handle nicely the case where there are more than one subscription
  $subscription = $subscriptions ? notifications_load_subscription(current($subscriptions)) : NULL;
  $objects = array('destination' => $destination, 'user' => $account, 'event' => $event, 'subscription' => $subscription);
  $objects = array_merge($objects, $event->get_objects());
  $text = messaging_template_text_replace($parts, $objects);

  // Get subject out of text and build the message array
  $message->subject = $text['subject'];
  unset($text['subject']);
  $message->body = $text;
  $message->text_parts = $parts;

  return $message;
}

