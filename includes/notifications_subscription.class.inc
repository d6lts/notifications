<?php
// $Id$
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Message destination class
 */
class Notifications_Subscription {
  // Blocked subscriptions, for blocked users
  const STATUS_BLOCKED = 0;
  // Enabled ones, will produce notifications
  const STATUS_ACTIVE = 1;
  // Temporarily disabled ones, maybe user on holidays
  const STATUS_INACTIVE = 2;
  // Unique subscription id
  public $sid;
  // User id this belongs to
  public $uid;
  // Destination id
  public $mdid;
  // Subscription type
  public $type;
  public $event_type;
  public $conditions;
  public $send_interval;
  public $send_method;
  public $cron = 1;
  public $module = 'notifications';
  public $status;
  public $destination = '';
  // Subscription fields
  protected $fields;
  // Temporary variables
  protected $account;
  protected $destination_object;
  /**
   * Class constructor
   */
  function __construct($type = NULL, $object = NULL) {
    $this->type = $type;
    $this->status = self::STATUS_ACTIVE;
    if ($object) {
      foreach ($object as $key => $value) {
        // Set old fields format (array of field => value)
        if ($key == 'fields') {
          $this->__set('fields', $value);
        }
        else {
          $this->$key = $value;
        }
      }
    }
    // Default values for new objects
    if (empty($this->sid)) {
      if (!isset($this->fields)) {
        $this->fields = array();
      }
      $this->created = time();
      if (!isset($this->conditions)) {
        $this->conditions = empty($this->fields) ? 0 : count($this->fields);
      }
      if (!isset($this->event_type) && !empty($this->type)) {
        $this->event_type = notifications_subscription_types($this->type, 'event_type');
      }
    }
    
  }
  /**
   * Build for a subscription type
   * 
   * @param $template
   *   Template object or subscription type
   */
  public static function build($template) {
    if (is_object($template)) {
      $object = $template;
      $type = $object->type;
    }
    else {
      $type = $template;
      $object = NULL;
    }
    // The type may define its own class
    if ($class = notifications_subscription_types($type, 'class')) {
      return new $class($type, $object);
    }
    else {
      return new Notifications_Subscription($type, $object);
    }
  }
  /**
   * Set user account
   */
  function set_account($account) {
    $this->account = $account;
    if ($account) {
      $this->uid = $account->uid;
    }
    if (!isset($this->send_interval)) {
      $this->send_interval = notifications_user_setting('send_interval', $account, 0);
    }
    if (!isset($this->send_method)) {
      $this->send_method = notifications_user_setting('send_method', $account);
    }
  }
  /**
   * Get user account
   */
  function get_account() {
    if (!isset($this->account) && isset($this->uid)) {
      $this->account = notifications_load_user($this->uid);
    }
    return isset($this->account) ? $this->account : NULL;
  }
  /**
   * Load from db
   */
  public static function load($sid, $full = TRUE) {
    if ($object = db_fetch_object(db_query("SELECT * FROM {notifications} WHERE sid = %d", $sid))) {
      $subscription = self::build($object);
      if ($full) {
        $subscription->load_fields();
      }
      return $subscription;
    }   
  }
  /**
   * Load condition fields from db
   */
  function load_fields() {
    $this->fields = array();
    if (!empty($this->sid)) {
      $result = db_query("SELECT * FROM {notifications_fields} WHERE sid = %d", $this->sid);
      while ($field = db_fetch_object($result)) {
        $this->set_field($field);
      }      
    }
  }
  /**
   * Save to db
   */
  function save() {
    $update = !empty($this->sid) ? 'sid' : array();
    $result = drupal_write_record('notifications', $this, $update);
    $this->save_fields($update);
    return $result; 
  }
  /**
   * Delete from db
   */
  function delete() {
    if (!empty($this->sid)) {
      notifications_delete_subscription($this->sid);
      unset($this->sid);
    }
  }
  /**
   * Save condition fields to db
   * 
   * @param $update
   *   Whether this is an old subscription being created
   */
  function save_fields($update = FALSE) {
    if (isset($this->fields)) {
      if ($update) {
        db_query("DELETE FROM {notifications_fields} WHERE sid = %d", $this->sid);
      }
      foreach ($this->fields as $field) {
        db_query("INSERT INTO {notifications_fields} (sid, field, value, intval) VALUES(%d, '%s', '%s', %d)", $this->sid, $field->name, $field->value, (int)$field->value);
      }
    }
  }
  /**
   * Add a new condition, update counter
   */
  function add_condition($name, $value) {
    $this->add_field($name, $value);
    $this->conditions++;
  }
  /**
   * Get fields as field => value array
   * 
   * Duplicate fields are returned as field => array(value1, value2...)
   */
  function get_conditions() {
    $list = array();
    foreach ($this->get_fields() as $field) {
      if (!isset($list[$field->field])) {
        $list[$field->field] = $field->value;
      }
      elseif (is_array($list[$field->field])) {
        $list[$field->field][] = $field->value;
      }
      else {
        $list[$field->field] = array($list[$field->field], $field->value);
      }
    }
    return $list;   
  }
  /**
   * Add a new field with name, value
   */
  function add_field($name, $value) {
    if (is_object($value)) {
      $field = $value;
    }
    else {
      $field = new Stdclass();
      $field->field = $name;
      $field->value = $value;
    }
    $this->set_field($field);
  }
  /**
   * Set a field
   */
  function set_field($field) {
    if (!isset($field->key)) {
      $field->key = isset($this->fields) ? count($this->fields) : 0;
    }
    if (!isset($field->type)) {
      $field->type = $field->field;
    }
    $this->fields[$field->key] = $field;
  }
  /**
   * Get fields as array of field objects
   */
  function get_fields() {
    if (!isset($this->fields)) {
      $this->load_fields();
    }
    return $this->fields;
  }
  /**
   * Get destination object
   */
  function get_destination() {
    if (!isset($this->destination_object)) {
      $destination = !empty($this->mdid) ? Messaging_Destination::load($this->mdid) : FALSE;
      $this->set_destination($destination);
    }
    return $this->destination_object;
  }
  /**
   * Set destination object
   */
  function set_destination($destination) {
    if (empty($destination)) {
      $this->mdid = 0;
      $this->destination = '';
      $this->destination_object = FALSE;
    }
    elseif (is_object($destination)) {
      $this->uid = $destination->uid;
      $this->mdid = $destination->mdid;
      $this->destination = $destination->address;
      $this->destination_object = $destination;
      $this->send_method = $destination->method;
    }
    elseif (is_numeric($destination)) {
      $this->mdid = $destination;
    }
  }
  /**
   * Get subscription type data
   */
  function get_type($property = NULL) {
    return notifications_subscription_types($this->type, $property); 
  }
  /**
   * Magic method, set protected properties
   */
  public function __set($name, $value) {
    switch ($name) {
      case 'fields':
        $this->fields = array();   
        foreach ($value as $key => $field_value) {
          $this->add_field($key, $field_value);
        }
        break;
    }
  }
  /**
   * Magic method, get protected properties
   */
  public function __get($name) {
    switch ($name) {
      case 'fields':
        return $this->get_conditions();
        break;
    }
  }
}