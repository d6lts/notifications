<?php
// $Id$
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Message destination class
 */
class Notifications_Subscription extends Messaging_Object {
  // Database properties
  const DB_TABLE = 'notifications';
  const DB_KEY = 'sid';
  // Blocked subscriptions, for blocked users
  const STATUS_BLOCKED = 0;
  // Enabled ones, will produce notifications
  const STATUS_ACTIVE = 1;
  // Temporarily disabled ones, maybe user on holidays
  const STATUS_INACTIVE = 2;
  // Unique subscription id
  public $sid = 0;
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
  // Language and temporary language object
  //public $language;
  //protected $_language;
  // Temporary variables
  protected $destination_object;
  protected $format;
  protected $objects;
  // Name for this subscription that will be quite dependent on context
  public $name;
  // Mark if incomplete loading of objects
  public $incomplete;
  
  /**
   * Class constructor
   */
  function __construct($object = NULL) {
    $this->status = self::STATUS_ACTIVE;
    if ($object) {
      foreach ($object as $key => $value) {
        // Set old fields format (array of field => value)
        if ($key == 'fields' || $key == 'destination') {
          $this->__set($key, $value);
        }
        else {
          $this->$key = $value;
        }
      }
    }
    // Default values for new objects
    if (empty($this->sid)) {
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
  public static function build($template, $default_class = 'Notifications_Subscription') {
    if (is_array($template)) {
      $type = NULL;
      $template = (object)$template;
    }
    if (is_object($template)) {
      $object = $template;
      $type = $template->type;
    } 
    else {
      $type = $template;
      $object = array('type' => $type);
    }
    // The type may define its own class
    if ($type && ($class = notifications_subscription_types($type, 'class'))) {
      return new $class($object);
    }
    else {
      return new $default_class($object);
    }
  }

  /**
   * Build from form submission
   */
  public static function build_submission($form_state) {
    // The object may be already created ir it may be an array so we use this function that handles that
    $subscription = notifications_build_subscription($form_state['values']['subscription']);
    // There may be optional fields to add
    if (!empty($form_state['values']['subscription_fields'])) {
      foreach ($form_state['values']['subscription_fields'] as $field) {
        if (isset($form_state['values'][$field])) {
          $subscription->$field = $form_state['values'][$field];
        }
      }
    }
    return $subscription;
  }
  /**
   * Set user account
   */
  function set_account($account) {
    $this->set_user($account);
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
    return $this->get_user();
  }
  /**
   * Load from db
   * 
   * @param $sid
   *   Subscription id 
   * @param $full
   *   Whether to load all fields in the same operation
   */
  public static function load($sid) {
    return self::object_load(self::DB_TABLE, self::DB_KEY, $sid, 'Notifications_Subscription');
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
  function insert() {
    $result = parent::insert();
    $this->save_fields(FALSE);
    return $result; 
  }
  /**
   * Update db
   */
  function update() {
    $this->save_fields(TRUE);
    return parent::update();
  }
  /**
   * Delete from db
   */
  function delete() {
    if ($this->is_instance()) {
      module_invoke_all('notifications_subscription', 'delete', $this);
      db_query("DELETE FROM {notifications_fields} WHERE sid = %d", $this->sid);
      return parent::delete();
    }
  }
  /**
   * Check destination or create it if new
   */
  function check_destination() {
    if (empty($this->mdid)) {
      if (!empty($this->destination_object)) {
        return $this->create_destination($this->destination_object->method, $this->destination_object->address);
      }
      else {
        return $this->create_destination();
      }
    }
    elseif ($destination = $this->get_destination()) {
      // We may need to create a new destination if method has changed
      if (!empty($this->send_method) && $destination->method != $this->send_method) {
        return $this->create_destination();
      }
      else {
        return $destination;
      }
    }
    else {
      // Last try, figure out a destination
      return $this->create_destination();
    }
  }
  /**
   * Create destination for this subscription
   */
  function create_destination($method = NULL, $address = NULL) {
    if ($method && $address) {
      $destination = Messaging_Destination::create($method, $address, $this->uid);
      $this->set_destination($destination);
      return $destination;
    }
    elseif (!empty($this->send_method) && !empty($this->destination)) {
      return $this->create_destination($this->send_method, $this->destination);
    }
    elseif (($account = $this->get_account()) && $account->uid) {
      // We still can create a destination if this has a user id
      $method = !empty($this->send_method) ? $this->send_method : messaging_method_default($account);
      if ($method && ($address = messaging_user_destination($account, $method))) {
        return $this->create_destination($method, $address);
      }
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
        db_query("INSERT INTO {notifications_fields} (sid, field, value, intval) VALUES(%d, '%s', '%s', %d)", $this->sid, $field->field, $field->value, (int)$field->value);
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
   * Get fields as array of field => value pairs
   * 
   * Duplicate fields are returned as field => array(value1, value2...)
   */
  function get_conditions() {
    $list = array();
    foreach ($this->get_fields() as $field) {
      // We cannot simply use isset() because the value may be NULL
      if (!array_key_exists($field->field, $list)) {
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
   * Order and serialize fields so we can get a unique signature for this subscription fields
   */
  function serialize_fields() {
    return notifications_array_serialize($this->get_conditions());
  }
  /**
   * Serialize type and conditions
   */
  function serialize_type() {
    return implode('/', array($this->type, $this->serialize_fields()));
  }
  /**
   * Add a new field with name, value
   */
  function add_field($type, $value = NULL) {
    $this->set_field($this->build_field($type, $value));
  }
  /**
   * Set a field. 
   * - If the field has already a key (index) it will replace an existing one with the same key
   * - If not key, it will be added to the field list with an ordinary key generated
   * 
   * @param $field
   *   Field object with one or more of these properties: key, type, field, value
   */
  function set_field($field, $replace = FALSE) {
    if (!isset($field->key) || !$replace) {
      $field->key = isset($this->fields) ? count($this->fields) : 0;
    }
    // Add to the fields array, may replace an old one
    $this->fields[$field->key] = $field;
  }
  /**
   * Get fields as array of field objects
   */
  function get_fields() {
    if (!isset($this->fields)) {
      if ($this->is_instance()) {
        $this->load_fields();
      }
      else {
        $this->set_fields();
      }
    }
    return $this->fields;
  }
  /**
   * Check all fields are there and optinally that they have a value
   */
  function check_fields($check_values = TRUE) {
    $this->set_fields();
    $instance_fields = array();
    foreach ($this->get_fields() as $field) {
      if ($check_values && !isset($field->value)) {
        return FALSE;
      }
      else {
        $instance_fields[] = $field->field;
      }
    }
    $type_fields = $this->get_type('fields');
    if (count($instance_fields) != count($type_fields)) {
      return FALSE;
    }
    else {
      // Order the arrays so we can compare them. Note that we can have the same field name more than once
      asort($instance_fields);
      asort($type_fields);
      return array_values($instance_fields) == array_values($type_fields);
    }
  }
  /**
   * Set field values, all at a time
   * 
   * Each field item may have different formats:
   * - Numeric key => field type. Like array(nid, tid)
   * - Numeric key => array('type' => type, 'value' => value)
   * - Numeric key => Object ($field object with field, value properties)
   * @param $fields
   *   Fields is an array of field items, each of them with many possible formats:
   *   NULL to set the fields from the subscription type or template if not set
   */
  function set_fields($fields = NULL, $normalize = TRUE) {
    if (isset($fields)) {
      $this->fields = array();
      if ($normalize) {
        $fields = $this->normalize_fields($fields);
      }
      foreach ($fields as $key => $value) {
        $this->set_field($value);
      }
    }
    elseif (!isset($this->fields)) {
      if ($fields = $this->get_type_fields()) {
        // Set the fields from the subscription type or template
        $this->set_fields($fields, FALSE);
      }
      else {
        // We have nothing better, set empty array()
        $this->fields = array();
      }
    }
  }
  /**
   * Get fields from subscription type as normalized array of objects
   */
  function get_type_fields() {
    return $this->normalize_fields($this->get_type('fields'));
  }
  /**
   * Convert fields to a standard format: arrray of object fields
   */
  public static function normalize_fields($fields) {
    $normal = array();
    if ($fields && is_array($fields)) {
      foreach ($fields as $key => $value) {
        if (is_object($value)) {
          $normal[] = $value;
        }
        elseif (is_array($value)) {
          // Object or Array with type, value keys
          $normal[] = self::build_field($value['type'], $value['value']);
        }
        elseif (is_numeric($key) && is_string($value)) {
          // In this case the value is the field type
          $normal[] = self::build_field($value, NULL);
        }
        else {
          // Default, key should be field type, value may be NULL
          $normal[] = self::build_field($key, $value);
        }
      }
    }
    return $normal;    
  }
  /**
   * Build field from type, value
   */
  public static function build_field($type, $value) {
    return (object)array(
      'field' => $type,
      'value' => $value,
    );
  }
  /**
   * Get destination object
   */
  function get_destination() {
    if (!isset($this->destination_object)) {
      $this->destination_object = !empty($this->mdid) ? Messaging_Destination::load($this->mdid) : FALSE;
    }
    return $this->destination_object;
  }
  /**
   * Get language object
   */
  function get_language() {
    if (!isset($this->_language)) {
      if (!empty($this->language) && ($languages = language_list()) && isset($languages[$this->language])) {
        $this->_language = $languages[$this->language];
      }
      else {
        $this->_language = user_preferred_language($this->get_account());
        $this->language = $this->_language->language;
      }
    }
    return $this->_language;
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
    return empty($this->type) ? NULL : notifications_subscription_types($this->type, $property);
  }

  /**
   * Check whether this is a subscription instance or just a type template
   */
  function is_instance() {
    return !empty($this->sid);
  }

  /**
   * Get instance of this one for certain conditions
   * 
   * @param $params
   *   Parameters to add to the subscription type to get an instance of itself
   * @param $return_self
   *   Return itself if no instance found
   */
  function get_instance($params = array(), $return_self = FALSE) {
    $params += array('type' => $this->type);
    $cache = &messaging_static('Notifications_Subscription_instances');
    $cache_key = notifications_array_serialize($params);
    if (!isset($cache[$cache_key])) {
      $subs = notifications_get_subscriptions($params);
      $cache[$cache_key] = $subs ? current($subs) : FALSE;      
    }
    return $cache[$cache_key] ? $cache[$cache_key] : ($return_self ? $this : NULL);
  }

  /**
   * Load subscription objects
   */
  function load_objects() {
    if (!isset($this->objects)) {
      $this->objects = array();
      foreach ($this->get_fields() as $field) {
        if ($type = notifications_subscription_fields($field->field, 'object_type')) {
          if ($object = notifications_object_load($type, $field->value)) {
            if (!isset($this->objects[$type])) {
              $this->objects[$type] = $object;
            }
            elseif (is_array($this->objects[$type])) {
              // Was an array, just add
              $this->objects[$type][] = $object;
            }
            else {
              // Was single element, make into an array with this new object
              $this->objects[$type] = array($this->objects[$type], $object);
            }
          }
          else {
            // Object cannot be loaded, mark as incomplete
            $this->incomplete = TRUE;
          }
        }
      } 
    }
    return empty($this->incomplete);
  }
  /**
   * Get objects
   */
  function get_objects() {
    $this->load_objects();
    return $this->objects;
  }
  /**
   * Format subscription for display
   * 
   * @return array()
   *   Array of formatted texts with the following properties
   *   - type, Subscription type name (title)
   *   - name, Subscription name or glued values if not name defined
   *   - names, Array of field names
   *   - values, Array of field values
   *   
   */
  function format_array($html = FALSE) {
    $fields = $this->format_fields($html ? 'html' : 'plain');
    return array(
      'type' => $this->get_type('title'),
      'name' => $this->format_name($html), 
      'names' => $fields['names'],
      'values' => $fields['values'],
    );
  }
  /**
   * Format as short text
   */
  function format_short($html = FALSE) {
    return t('@type: !values', array('@type' => $this->get_type('title'), '!values' => $this->format_name($html)));
  }
  /**
   * Format as long text
   */
  function format_long($html = FALSE) {
    return t('Subscription %id of type %type to: !values', array('%id' => $this->sid, '%type' => $this->get_type('title'), '!values' => $this->format_name($html)));
  }
  
  /**
   * Get subscription short name.
   */
  function get_name() {
    if (!isset($this->name)) {
      // This subscription doesn't have a name, we'll find something
      $this->name = $this->format_name(FALSE);
    }
    return $this->name;
  }
   /**
    * 
   * If the subscription type has a name, like custom subscriptions have, that will be the name
   * Otherwise we build the name using fields and values
   */
  function format_name($html = FALSE) {
    if ($name = $this->get_type('name')) {
      return $name;
    }
    else {
      $fields = $this->format_fields($html ? 'html' : 'plain');
      return $fields['values'] ? implode(', ', $fields['values']) : t('Unknown (no fields)');
    }   
  }
  
  /**
   * Format all fields
   *
   * @return array();
   *   Array of arrrays with (name, value)
   */
  function format_fields($format = 'plain') {
    if (!isset($this->format[$format])) {
      $html = ($format == 'html');
      // Get field names and values formatting each field
      $this->format[$format]['names'] = array();
      $this->format[$format]['values'] = array();
      foreach ($this->get_fields() as $field) {
        $item = $this->format_field($field->field, $field->value, $html, $this->type);
        $this->format[$format]['names'][] = $item['name'];
        $this->format[$format]['values'][] = $item['value'];
      }      
    }
    return $this->format[$format];
  }

  /**
   * Format subscriptions field for display and get some more information
   * 
   * @return array()
   *   Array with 'name' and 'value' elements
   */
  public static function format_field($type, $value, $html = TRUE, $subscription_type = NULL) {
    notifications_include('object.inc');
    $format_name = $format_value = t('Unknown');
    
    if ($info = notifications_subscription_fields($type)) {
      $format_name = $info['name'];
      // First try format callback
      $format_value = _notifications_field_callback($type, 'format callback', $value, $html, $subscription_type);
      // If not we try options callback, we can get the name from the array of options
      if (!isset($format_value)) {
        $options = _notifications_field_callback($type, 'options callback', $subscription_type);
        if (isset($options)) {
          $format_value = isset($options[$value]) ? $options[$value] : t('Not available');
        }
      }
      // If nothing got, we return the value
      if (!isset($format_value)) {
        $format_value = check_plain($value);
      }
    }
  
    return array('name' => $format_name, 'value' => $format_value);
  }

  /**
   * Subscription information field for several forms
   */
  function form_info() {
    $info = $this->get_type();
    //$format = $subscription->format_array(TRUE);

    $fields = $this->format_fields($html ? 'html' : 'plain');
    $format = array(
      'type' => $this->get_type('title'),
      'name' => $this->format_name($html), 
      'names' => $fields['names'],
      'values' => $fields['values'],
    );
    if (!empty($info['name'])) {
      // This subscription type already have a name
      $value = $info['name'];
    }
    elseif (empty($format['names'])) {
      // No name, maybe no fields it should be enough with the title
      $value = '';
    }
    elseif (count($format['names']) == 1) {
      // If the field is unique, we don't need a table nor a name for it
      $value = array_shift($format['values']);
    }
    else {
      // Multiple fields, format as a table
      foreach ($format['names'] as $index => $value) {
        $rows[] = array(
          $value,
          $format['values'][$index],
        );
      }
      $value = theme('table', array(), $rows);
    }
    // Build a form field
    $field = array(
      '#type' => 'item',
      '#title' => t('!type subscription', array('!type' => $format['type'])),
      '#value' => $value,
    );
    if (!empty($info['description'])) {
      $field['#description'] = $info['description'];
    }
    return $field;
  }
  /**
   * Display a form field for a notifications_field
   */
  public static function form_field($type, $value = NULL, $subtype = NULL) {
    $field_info = notifications_subscription_fields($type);
    $object_info = isset($field_info['object_type']) ? notifications_object_type($field_info['object_type']) : array();
    $merged_info = $field_info + $object_info;
  
    if (isset($merged_info['options callback'])) {
      $element['#type'] = 'select';
      $element['#options'] = _notifications_field_callback($type, 'options callback');
    }
    elseif (!empty($merged_info['autocomplete path'])) {
      $element['#type'] = 'textfield';
      $element['#autocomplete_path'] = $merged_info['autocomplete path'];
      if ($value) {     
        if (!empty($merged_info['autocomplete callback'])) {
          $value = _notifications_field_callback($type, 'autocomplete callback', $value, $subtype);
        }
        elseif (!empty($merged_info['format callback']))  {
          $value = _notifications_field_callback($type, 'format callback', $value, FALSE, $subtype);
        }      
      }
    }
    else {
      $element['#type'] = 'textfield';
      if ($value) {
        $value = check_plain($value);
      }
    }
    if ($value) {
      $element['#default_value'] = $value;
    }
    return $element;
  }
  
  /**
   * Magic method, set protected properties
   * 
   * Warning: This also runs for unset properties
   */
  public function __set($name, $value) {
    switch ($name) {
      case 'fields':
        $this->set_fields($this->normalize_fields($value));
        break;
      case 'destination':
        if (is_object($value)) {
          $this->set_destination($value);
        }
        else {
          $this->destination = $value;
        }
        break;
      default:
        $this->$name = $value;
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
  // Get table name for storage
  public static function db_table() {
    return self::DB_TABLE;
  }
  // Get key field name for storage
  public static function db_key() {
    return self::DB_KEY;
  }
}