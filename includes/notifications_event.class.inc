<?php
// $Id$
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Message destination class
 */
class Notifications_Event {
  // Object unique id
  public $eid;
  public $module;
  public $type;
  public $action;
  // Main object id
  public $oid;
  public $language;
  // User id that caused the event
  public $uid;
  // Sending method
  public $params;
  // Address for this sending method
  public $created;
  // Notifications in queue linked to this event
  public $counter = 0;
  // Loaded objects for this event
  public $objects = array();
  // Processing options, not saved
  public $save = TRUE;
  public $queue = TRUE;
  public $delete;
  // Event text for composition
  public $text;

  /**
   * Constructor
   */
  function __construct($object = NULL) {
    if ($object) {
      foreach ($object as $field => $value) {
        $this->$field = $value;
      }
    }
    if (!isset($event->created)) {
      $event->created = time();
    }
  }
  /**
   * Get set event object
   */
  function object($type, $object = NULL) {
    if (isset($object)) {
      $this->objects[$type] = $object;
    }
    else {
      return isset($this->objects[$type]) ? $this->objects[$type] : NULL;
    }
  }
  /**
   * Add event object
   */
  function add_object($type, $object, $key) {
    $this->params[$type . ':' . $key] = $object->$key;
    $this->set_object($type, $object);
  }
  function set_object($type, $object) {
    $this->objects[$type] = $object;
  }
  /**
   * Save/update the event
   */
  function save() {
    $update = empty($this->eid) ? array() : 'eid';
    drupal_write_record('notifications_event', $this, $update);    
  }
  /**
   * Delete from system
   */ 
  function delete() {
    db_query('DELETE FROM {notifications_event} WHERE eid = %d', $this->eid);
    unset($this->eid);
  }
  /**
   * Load by id
   */
  public static function load($eid) {
    if ($object = db_fetch_object(db_query("SELECT * FROM {notifications_event} WHERE eid = %d", $eid))) {
      $object->params = $object->params ? unserialize($object->params) : array();
      $event = new Notifications_Event($object);
      // Load aditional objects for the event
      notifications_module_invoke('event load', $event);
      return $event;
    }
  }
  /**
   * Track notifications queue row processed, decrease counter
   */
  function track_count() {
    return $this->counter ? --$this->counter : 0;
  }
  /**
   * Update event counter
   */
  function update_counter($value = NULL) {
    if (isset($value)) {
      $event->counter = $value;
    }
    db_query('UPDATE {notifications_event} SET counter = %d WHERE eid = %d', $event->counter, $this->eid);
  }
}