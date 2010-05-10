<?php
// $Id$
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Message destination class
 */
class Notifications_Event extends Messaging_Cached_Object {
  // Database properties
  const DB_TABLE = 'notifications_event';
  const DB_KEY = 'eid';
  // Object unique id
  public $eid;
  public $module = 'notifications';
  public $type = '';
  public $action = '';
  // Event key that will define event behavior
  public $typekey;
  // Main object id
  public $oid;
  // Mixed array of parameters
  public $params = array();
  // Time the event was produced
  public $created;
  // Notifications in queue linked to this event
  public $counter = 0;
  // Loaded objects for this event
  public $objects;
  // Processing options, not saved
  public $save = TRUE;
  public $queue = TRUE;
  public $queued = FALSE;
  // Will be set if any of the objects cannot be loaded
  public $incomplete = FALSE;
  // Event text for composition
  public $text;
  // Template to use for this event
  protected $template;
  // Temporary storage for digesting information
  public $digest;
  /**
   * Constructor
   */
  function __construct($object = NULL) {
    parent::__construct($object);
    if (!isset($this->created)) {
      $this->created = time();
    }
    if (!isset($this->typekey)) {
      $this->typekey = $this->type . '-' . $this->action;
    }
  }
  /**
   * Building function
   */
  public static function build($template) {
    return new Notifications_Event($template);
  }
  /**
   * Get event type information
   */
  function get_type($property = NULL) {
    if ($info = notifications_event_types($this->typekey)) {
      if ($property) {
        return isset($info[$property]) ? $info[$property] : NULL;
      }
      else {
        return $info;
      }
    }
  }
  /**
   * Get template name
   */
  function get_template() {
    if (!isset($this->template)) {
      if ($template = notifications_event_get_template($this->typekey)) {
        $this->template = $template;
      }
      else {
        // Build default that will be like notifications-event-type-action
        $parts = array($this->module, 'event', $this->type, $this->action);
        $this->template = implode('-', array_filter($parts));
      }
    }
    return $this->template;
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
   * Add event object to be serialized and stored with the event
   */
  function store_object($type, $object) {
    $this->params['objects'][$type] = $object;
    $this->set_object($type, $object);
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
   * Get single object
   */
  function get_object($type) {
    $this->load_objects();
    return isset($this->objects[$type]) ? $this->objects[$type] : NULL;
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
          if (is_object($value)) {
            // This object was serialized along with the event, just set it
            $this->objects[$type] = $object;
          }
          elseif ($object = notifications_object_load($type, $value)) {
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
   * Load by id
   */
  public static function load($key) {
    return self::object_load(self::DB_TABLE, self::DB_KEY, $key, 'Notifications_Event');
  }
  /**
   * Unserialize after db loading
   */
  public function unserialize() {
    $this->params = $this->params ? unserialize($this->params) : array();
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
      $this->counter = $value;
    }
    db_query('UPDATE {notifications_event} SET counter = %d WHERE eid = %d', $this->counter, $this->eid);
  }
  // Store into cache
  public static function cache_set($key, $object) {
    return self::object_cache_set('Notifications_Event', $key, $object);
  }
  // Get from cache
  public static function cache_get($key) {
    return self::object_cache_get('Notifications_Event', $key);
  }
  // Get table name for storage
  public static function db_table() {
    return self::DB_TABLE;
  }
  // Get key field name for storage
  public static function db_key() {
    return self::DB_KEY;
  }
}