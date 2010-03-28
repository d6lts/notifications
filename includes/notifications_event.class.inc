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
  public $objects;
  // Processing options, not saved
  public $save = TRUE;
  public $queue = TRUE;
  // Will be set if any of the objects cannot be loaded
  public $incomplete = FALSE;
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
   * Get event type information
   */
  function get_type($property = NULL) {
    if ($info = notifications_event_types($this->type, $this->action)) {
      if ($property) {
        return isset($info[$property]) ? $info[$property] : NULL;
      }
      else {
        return $info;
      }
    }
  }
  /**
   * Get event text if available
   */
  function get_text($key) {
    if (isset($this->text[$key])) {
      return $this->text[$key];
    }
    elseif (isset($this->params['text'][$key])) {
      return $this->params['text'][$key];
    }
  }
  /**
   * Get set event object
   */
  function object($type, $object = NULL) {
    if (isset($object)) {
      $this->add_object($type, $object);
    }
    else {
      $this->load_objects();
      return isset($this->objects[$type]) ? $this->objects[$type] : NULL;
    }
  }
  /**
   * Add event object
   * 
   * @param $type
   *   Object type: 'user', 'node', etc...
   * @param $object
   *   Full object or key field value. I.e. it may be nid, uid
   */
  function add_object($type, $object) {
    $key = notifications_object_type($type, 'key_field');
    $value = is_object($object) ? $object->$key : $object;
    $this->params['objects'][$type] = $value;
    if (is_object($object)) {
      $this->set_object($type, $object);
    }
  }
  /**
   * Set event object
   */
  function set_object($type, $object) {
    $this->objects[$type] = $object;
  }
  /**
   * Get event objects
   */
  function get_objects() {
    // Make sure objects are loaded
    $this->load_objects();
    return $this->objects;
  }
  /**
   * Load event objects
   * 
   * @return Boolean
   *   Object status, FALSE for incomplete (missing object)
   */
  function load_objects($refresh = FALSE) {
    if (!isset($this->objects) || $refresh) {
      $this->objects = array();
      if (!empty($this->params['objects'])) {
        foreach ($this->params['objects'] as $type => $value) {
          if ($object = notifications_object_load($type, $value)) {
            $this->objects[$type] = $object;
          }
          else {
            // Cannot find object, mark as incomplete and return FALSE
            $this->incomplete = TRUE;
            return FALSE;
          }
        }
      }
    }
    // Return true unless marked as incomplete
    return $this->incomplete ? FALSE : TRUE;
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
      return new Notifications_Event($object);
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