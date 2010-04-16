<?php
// $Id$
/**
 * @file
 * Notifications object and fields
 * 
 * This is a library for handling objects and fields defined by notifications hooks
 */

/**
 * Check access to an object for user account
 * 
 * @param $type
 *   Object type
 * @param $object
 *   Object or object id
 * @param $account
 *   User account to check access to the object
 */
function notifications_object_access($type, $object, $account) {
  $object = notifications_object_load($type, $object);
  // If object not properly loaded, always false
  if (!$object) {
    return FALSE;
  }
  elseif (($info = notifications_object_type($type)) && ($key = $info['key_field']) && isset($object->$key)) {
    $access = &messaging_static(__FUNCTION__);
    if (!isset($access[$type][$account->uid][$object->$key])) {
      if (isset($info['access callback'])) {
        $access[$type][$account->uid][$object->$key] = _notifications_object_callback($type, 'access callback', $object, $account);
      }
      elseif (isset($info['access'])) {
        $access[$type][$account->uid][$object->$key] = user_access($info['access'], $account);
      }
      else {
        // Not defined, so we allow user access
        $access[$type][$account->uid][$object->$key] = TRUE;
      }
    }
    return $access[$type][$account->uid][$object->$key];
  }
  // If not object information we cannot determine anything 
}

/**
 * Get subscription options for object, account
 */
function notifications_object_subscribe_options($type, $object, $account = NULL) {
  $account = $account ? $account : $GLOBALS['user'];
  $object = notifications_object_load($type, $object);
  return module_invoke_all('notifications_object_' . $type, 'subscriptions', $object, $account);
}

/**
 * Build subscribe / unsubscribe options for object
 */
function notifications_object_subscribe_links($type, $object, $account, $subscribe_options = array(), $unsubscribe_options = array()) {
  $links = array();
  if ($subscriptions = notifications_object_user_subscriptions($type, $object, $account)) {
    foreach ($subscriptions as $index => $subscription) {
      $options = $subscription->is_instance() ? $unsubscribe_options : $subscribe_options;
      if ($link = $subscription->build_link($options)) {
        $links['notitications_' . $index] = $link;
      }
    }
  }
  return $links;
}

/**
 * Get field conditions for this specific object
 */
function notifications_object_condition_fields($type, $object) {
  if ($object = notifications_object_load($type, $object)) {
    // As this does an array_merge_recursive() we get grouped field => array(value1, value2..)
    $fields = module_invoke_all('notifications_object_' .$type, 'conditions', $object);
    // Now we just need to filter out duplicate values
    foreach ($fields as $key => $value) {
      if (is_array($value)) {
        $fields[$key] = array_unique($value);
      }
    }
    return $fields;
  }
}

/**
 * Get list of possible and existing subscriptions for user/object
 * 
 * @param $type
 *   Subscription type to get options: 'user', 'node'
 * @param $object
 *   The object to subscribe. It may be $node or $user
 * @param $account
 *   User account to get options/subscriptions for
 * 
 * @return
 *   Array of subscription options
 *   The enabled ones will have a 'subscriptions' element loaded
 */
function notifications_object_user_subscriptions($type, $object, $account = NULL) {
  $cache = &messaging_static(__FUNCTION__);
  $account = $account ? $account : $GLOBALS['user'];
  $object = notifications_object_load($type, $object);
  
  // Get allowed subscription options for this account to this object
  $subscribe_options = notifications_object_subscribe_options($type, $object, $account);
  $allowed_options = array();
  foreach ($subscribe_options as $option) {
    // So far this is not a subscription but a subscription template
    $subscription = notifications_build_subscription($option);
    $type_key = $subscription->serialize_type();
    // If we have this type cached we don't search more
    if (!isset($cache[$account->uid][$type_key])) {
      if (notifications_user_allowed('subscription', $account, $subscription)) {
        $subscription->set_account($account);
        // If anonymous user we don't search more because we cannot find by uid
        if ($account->uid) {
          $find = notifications_get_subscriptions(
            array('uid' => $account->uid, 'type' => $subscription->type),
            $subscription->get_conditions()
          );
          // Allowed subscription type, we store the subscription or the template
          if ($find) {
            $usersubs = current($find);
            $usersubs->name = $subscription->name;
            $subscription = $usersubs;
          }
        }
        $cache[$account->uid][$type_key] = $subscription;
      }
      else {
        // Not allowed subscription type for this user
        $cache[$account->uid][$type_key] = FALSE;
      }
    }
    if ($cache[$account->uid][$type_key]) {
      $allowed_options[] = $cache[$account->uid][$type_key];
    }
  }
  return $allowed_options;
}


/**
 * Get full info array for field_type that has the property
 * 
 * @param $type
 *   Field type
 * @param $property
 *   Property we are looking for
 * 
 * @return array()
 *   Info array from field or from object type
 */
function notifications_field_info($type, $property) {
  if ($info = notifications_field_type($type)) {
    if (isset($info[$property])) {
      return $info;
    }
    elseif (!empty($info['object_type']) && notifications_object_type($info['object_type'], $property)) {
      return notifications_object_type($info['object_type']);
    }
  }
}

/**
 * Format field value
 */
function notifications_field_format_value($type, $value, $html = TRUE, $subscription_type = NULL) {
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

/**
 * Collect submitted fields and parse new values
 */
function notifications_field_parse_submitted(&$form_state, $element_name = 'fields') {
  $fields = array();
  if (!empty($form_state['values'][$element_name]['type'])) {
    $field_values = &$form_state['values'][$element_name];
    foreach ($field_values['type'] as $key => $type) { 
      // If marked for deletion we just keep it there, don't return field
      if (empty($field_values['delete'][$key])) {
        // First collect all field values from the form   
        $field = array('type' => $type, 'value' => $field_values['value'][$key], 'edit' => $field_values['edit'][$key]);  
        // Complete field edit value, depending on field definition.
        if (empty($field_values['parsed'][$key])) {
          $value = notifications_field_real_value($type, $field['edit']);
          if (isset($value)) {
            $field['value'] = $value;
            $field_values['value'][$key] = $value;
            $field['parsed'] = TRUE;
            $field_values['parsed'][$key] = TRUE;
          }
          // Otherwise we let the field keep its value
        }
        // Add field to the list and mark as formatted so we can use this value for the form
        $fields[] = $field;
      }
    }
    
  }
  return $fields;
}

/**
 * Validate submitted field values and set the new ones as valid array of values
 */
function notifications_field_validate_submitted(&$form_state, $element_name = 'fields', $require_one = TRUE, $require_all = TRUE) {
  $checked_values = array();
  if ($field_values = notifications_field_parse_submitted($form_state, $element_name)) {
    foreach ($field_values as $key => $field) {
      $string_id = "$element_name][edit][$key";
      // We validate the field, type included
      if (notifications_field_valid_value($field['edit'])) {
        if (empty($field['parsed']) || !notifications_field_valid_value($field['value'], $field['type'])) {
          form_set_error($string_id, t('The value for this field is not valid.'));
          continue;
        }
      }
      elseif ($require_all) {
        form_set_error($string_id, t('You must set a value for this field.'));
        continue;
      }
      $checked_values[] = array('type' => $field['type'], 'value' => $field['value']);
    }
  }
  elseif ($require_one) {
    form_set_error(NULL, t('You must set at least one field for this subscription type.'));
  }
  return $checked_values;
}

/**
 * Convert field value from submission into its real value
 */
function notifications_field_real_value($type, $value) {  
  if (!notifications_field_valid_value($value)) {
    return NULL;
  }
  elseif ($info = notifications_field_info($type, 'value callback')) {
    // We have a value callback for field or object so use it
    return _notifications_info_callback($info, 'value callback', array($value));
  }
  else {
    // As we have nothing better, return the value itself
    return $value;
  }
}

/**
 * Get type information for field. For now its just subscription fields
 */
function notifications_field_type($type = NULL, $property = NULL) {
  return notifications_subscription_fields($type, $property);
}

/**
 * Check if the field has a valid value
 */
function notifications_field_valid_value($value, $type = NULL) {
  // A numeric value of zero is possible too, that's why the is_numeric()
  if (!is_numeric($value) && empty($value)) {
    // The field has no value at all, no go
    return FALSE;
  }
  elseif ($type) {
    // We want aditional field type validation
    switch (notifications_field_type($type, 'type')) {
      case 'int':
        // @todo Better integer validation, is_int not working for strings
        return is_numeric($value);
      case 'float':
        return is_numeric($value);
      case 'string':
      default:
        return is_string($value);
    }
  }
  else {
    return TRUE;
  }
}

