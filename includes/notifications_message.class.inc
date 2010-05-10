<?php
// $Id$
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Wrapper for Notifications messages
 * 
 * Store some more temporary properties on the message that will be dropped if the message is stored
 */
class Notifications_Message extends Messaging_Message {
  // Message type, set property
  public $type = 'notifications';
  // Send interval used for grouping
  public $send_interval = 0;  
  // Notifications events
  public $events = array();
  // Map events to subscriptions
  public $events_subscriptions = array();
  // Subscriptions that triggered this message, indexed by sid
  public $subscriptions = array();
  // Text parts used to build this, will be useful for debugging
  public $text_parts = array();
  // Digest flag, will be true if this is a digest of multiple events
  public $digest = FALSE;
  // Build method used for this
  public $build_method;

  
  /**
   * Check parameters and go for alter hook
   */
  public function message_build() {  
    if (empty($this->built)) {
      // Provide a specific notifications alter hook
      drupal_alter('notifications_message', $this);
      // Clean up Notifications objects so we don't move too much data around (Sometimes the full object gets serialized)
      unset($this->events);
      unset($this->events_subscriptions);
      unset($this->subscriptions);
      unset($this->text_parts);
    }
    return parent::message_build();
  }
  
  /**
   * Add event to the message. One message can be produced by one or more events.
   */
  public function add_event($event, $subscriptions = array()) {
    $this->events[$event->eid] = $event;
    $this->events_subscriptions[$event->eid] = $subscriptions;
    $this->add_subscriptions($subscriptions);
  }
  
  /**
   * Add subscriptions
   */
  public function add_subscriptions($subscriptions) {
    foreach ($subscriptions as $sid) {
      if (!isset($this->subscriptions[$sid])) {
        $this->subscriptions[$sid] = $sid;
      }
    }
  }
  /**
   * Set message sender. Depending on options this will set sender account too.
   * 
   * Optional sender, if chosen will be the user account who produced the event
   * It will be up to the sending method modules what to do with this information.
   */
  public function set_sender($account) {
    $sender_option = variable_get('notifications_sender', 0);
    if ($sender_option) {
      $sender = is_numeric($account) ? notifications_load_user($account) : $account;
      if ($sender_option == 2) {
        parent::set_sender($sender);
      }
      else {
        $this->sender = 0;
        $this->sender_name = $sender->name;
      }
    }
  }
}