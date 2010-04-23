<?php
// $Id$
/**
 * @file
 *   Destination management
 */

/**
 * Query subscriptions for destination
 */
function notifications_destination_get_subscriptions($destination, $pager = 0) {
  return notifications_get_subscriptions(array('mdid' => $destination->mdid), array(), FALSE, 'sid', $pager);
}

/**
 * Validate destination   
 */
function notifications_destination_validate($account, $method, $address) {
  // Check the method is valid for this account
  $valid_methods = notifications_send_methods($account);
  return isset($valid_methods[$method]) && Messaging_Destination::validate($method, $address, $account);
}

/**
 * Subform elements for destination data
 */
function notifications_destination_subform($destination, $links = array()) {
  // Count subscriptions for this destination
  $count = db_result(db_query("SELECT COUNT(*) FROM {notifications} WHERE mdid = %d", $destination->mdid));
  $form['destination'] = array('#type' => 'value', '#value' => 'destination');
  $form['destination_view'] = array(
    '#title' => $destination->address_name(),
    '#type' => 'item',
    '#value' => $destination->format_address(TRUE),
    '#description' => format_plural($count, "There is one subscription for this destination.", 'There are @count subscriptions for this destination.'),
  );
  if ($links) {
    $form['destination_options'] = array(
      '#type' => 'item',
      '#value' => implode(' | ', $links),
    );
  }
  return $form;
}
/**
 * Subform with method and address
 */
function notifications_destination_method_subform($account, $destination = NULL) {
  $form['destination_account'] = array('#type' => 'value', '#value' => $account);
  $send_methods = notifications_send_methods($account);
  $form['destination_method'] = array(
    '#type' => 'fieldset',
    '#tree' => TRUE,
  );
  $form['destination_method']['method'] = array(
    '#type' => 'select',
    '#title' => t('Send method'),
    '#options' => $send_methods,
    '#default_value' => $destination ? $destination->method : '',
    '#disabled' => count($send_methods) < 2,
  );
  $form['destination_method']['address'] = array(
    '#type' => 'textfield',
    '#title' => t('Address'),
    '#size' => 40,
    '#required' => TRUE,
    '#default_value' => $destination ? $destination->address : '',
  );
  return $form;
}

/**
 * Displays a destination field for each method
 * 
 * @param $account
 *   User account the destination is for
 * @param $destination
 *   Destination object to populate one of the addresses
 * @param $send_methods
 *   Array method => name to restrict the number of sending methods
 * @param $size
 *   Address text field size
 */
function notifications_destination_address_subform($account, $destination = NULL, $send_methods = NULL, $size = 40) {
  $send_methods = isset($send_methods) ? $send_methods : notifications_send_methods($account);
  if (!$send_methods) {
    $form['warning']['#value'] = '<p>' . t('No sending methods available.') . '</p>';
    return $form;
  }
  $form['destination_account'] = array('#type' => 'value', '#value' => $account);
  $form['destination_address'] = array(
    '#type' => 'fieldset',
    '#title' => t('Destination'),
    '#tree' => TRUE,
  );
  if (count($send_methods) > 1) {
    $form['destination_address']['#description'] = t('Enter only a destination address.');
  }
  foreach ($send_methods as $key => $method_name) {
    // We use method name if we don't have address name
    $type = messaging_method_info($key, 'address_type');
    $name = $type ? messaging_address_info($type, 'name') : $method_name;
    $form['destination_address'][$key] = array(
      '#title' => $name,
      '#type' => 'textfield',
      '#size' => $size,
      '#default_value' => $destination && $destination->method == $key ? $destination->address : '',
    );
  }
  return $form;
}

/**
 * Parse submitted destination
 */
function notifications_destination_parse_submitted($values, $validate = FALSE) {
  $method = $address = NULL;
  if (!empty($values['destination_account'])) {
    $account = $values['destination_account'];
    $uid = $account->uid;
  }
  else {
    $accoun = drupal_anonymous_user();
    $uid = 0;
  }
  // Destination can come in any of these configurations
  if (!empty($values['destination_method'])) {
    $field = 'destination_method';
    $method = $values['destination_method']['method'];
    $address = $values['destination_method']['address'];
  }
  elseif (!empty($values['destination_address'])) {
    $field = 'destination_address';
    $destination = $values['destination_address'];
    if (is_array($destination)) {
      $destination = array_filter($destination);
      if ($destination && count($destination) == 1) {
        $method = key($destination);
        $address = current($destination);
      }
    }    
  }
  if ($validate) {
    if (!$method || !$address) {
      form_set_error($field, t('You need to enter exactly one destination address.'));
    }
    elseif (!notifications_destination_validate($account, $method, $address)) {
      form_set_error($field, t('This destination address is not valid.'));
    }
  }
  return array($method, $address, $uid);
}

/**
 * Edit destination form
 */
function notifications_destination_edit_form($form_state, $destination, $options = array()) {
  global $user;
  $form = notifications_destination_subform($destination, $options);
  $form += notifications_destination_method_subform($user, $destination);
  $form['update'] = array('#type' => 'submit', '#value' => t('Update'));
  //$form['delete'] = array('#type' => 'submit', '#value' => t('Delete'));
  return $form;
}
/**
 * Edit destination validate
 */
function notifications_destination_edit_form_validate($form, $form_state) {
  notifications_destination_parse_submitted($form_state['values'], TRUE);
}

/**
 * Edit destinatin submission
 */
function notifications_destination_edit_form_submit($form, $form_state) {
  $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : NULL;
  $destination = $form_state['values']['destination'];
  switch ($op) {
    case t('Update'):
      list($method, $address, $uid) = notifications_destination_parse_submitted($form_state['values']);
      $destination->method = $method;
      $destination->address = $address;
      $object = Messaging_Destination::build($destination);
      $object->save();
      drupal_set_message(t('The destination address has been updated.'));
      break;
  }
}

/**
 * Form for unsubscription confirmation
 * 
 * It works for both single subscription or account (all subscriptions)
 */
function notifications_destination_delete_confirm($form_state, $destination, $options = array()) {
  // Pass on destination values and display them
  $form = notifications_destination_subform($destination, $options);

  return confirm_form($form,
    t('Are you sure you want to delete this destination and all its subscriptions?'),
    isset($_GET['destination']) ? $_GET['destination'] : '',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Destination deletion confirmed
 */
function notifications_destination_delete_confirm_submit($form, $form_state) {
  if ($destination = $form_state['values']['destination']) {
    notifications_destination_delete($destination->mdid);
    drupal_set_message('The destination and all its subscriptions have been deleted.');
  }
}

/**
 * Unsubscribe form
 */
function notifications_destination_unsubscribe_form($form_state, $destination, $options = array()) {
  $form = notifications_destination_subform($destination, $options);
  return confirm_form($form,
    t('This will delete this address and all its subscriptions.'),
    isset($_GET['destination']) ? $_GET['destination'] : '',
    t('This action cannot be undone.'),
    t('Unsubscribe'),
    t('Cancel')
  );
}

/**
 * Unsubscribe form submit
 */
function notifications_destination_unsubscribe_form_submit($form, $form_state) {
  $destination = $form_state['values']['destination'];
  notifications_delete_destination($destination->mdid);
  drupal_set_message(t('The destination and all its subscriptions have been deleted.'));
}

/**
 * Destination manage subscriptions form
 */
function notifications_destination_manage_form($form_state, $destination, $options = array()) {
  module_load_include('inc', 'notifications', 'notifications.pages');
  $account = $destination->get_account();
  if (isset($form_state['values']['operation']) && $form_state['values']['operation'] == 'delete') {
    $form = notifications_multiple_delete_confirm($form_state, array_filter($form_state['values']['subscriptions']));
    return $form;
  }  
  $form['description'] = notifications_destination_subform($destination, $options);
  $form['admin']  = notifications_destination_manage_subform($destination);
  $form['admin'] +=  notifications_destination_manage_subscriptions_form_options($account);
  //$form['options'] = array('#type' => 'fieldset', '#title' => t('Options'));
  return $form;
}

/**
 * Destination manage subform. List/edit subscriptions for destination.
 */
function notifications_destination_manage_subform($destination) {
  module_load_include('manage.inc', 'notifications');
  $subscriptions = notifications_destination_get_subscriptions($destination, 20);
  // List of subscriptions for selection
  $select = array();
  $status = _notifications_subscription_status();
  $send_methods = messaging_method_info(NULL, 'name');
  $send_intervals = notifications_send_intervals();
  $drupal_destination = drupal_get_destination();
  foreach ($subscriptions as $subs) {
    $select[$subs->sid] = '';
    $form['type'][$subs->sid] = array('#value' => notifications_subscription_types($subs->type, 'title'));
    $form['description'][$subs->sid] = array('#value' => $subs->get_name());
    $form['send_interval'][$subs->sid] = array('#value' => !empty($send_intervals[$subs->send_interval]) ? $send_intervals[$subs->send_interval] : $subs->send_interval);  
    $form['status'][$subs->sid] =  array('#value' => $status[$subs->status]);
    $operations = array();
    if ($destination->uid || user_access('administer notifications')) {
      // Links for subscription for user. Permissions will be checked later.
      $operations[] = l(t('edit'), 'notifications/subscription/' . $subs->sid, array('query' => $drupal_destination));
      $operations[] = l(t('drop'), 'notifications/unsubscribe/sid/' . $subs->sid, array('query' => $drupal_destination));
    }
    elseif (function_exists('notifications_anonymous_manage_links')) {
      $operations[] = notifications_anonymous_manage_links('subscription', $subs);
    }
    $form['operations'][$subs->sid] = array('#value' => implode(', ', $operations));
  }
  $form['subscriptions'] = array('#type' => 'checkboxes', '#options' => $select);
  $form['pager'] = array('#value' => theme('pager', NULL, 20, 0));
  $form['#theme'] = 'notifications_manage_subscriptions';
  return $form;
}

/**
 * Form options
 */
function notifications_destination_manage_subscriptions_form_options($account) {
  $form['options'] = array(
    '#type' => 'fieldset',
    '#title' => t('Update options'),
    '#prefix' => '<div class="container-inline">',
    '#suffix' => '</div>',
  );
  $options = array();
  foreach (notifications_destination_subscriptions_operations($account) as $operation => $array) {
    if (!empty($array['parent'])) {
      $options[$array['parent']][$operation] = $array['label'];
    } else {  
      $options[$operation] = $array['label'];
    }
  }
  $form['options']['operation'] = array(
    '#type' => 'select',
    '#options' => $options,
    '#default_value' => 'approve',
  );
  $form['options']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Update'),
    '#validate' => array('notifications_manage_subscriptions_form_validate'),
    '#submit' => array('notifications_manage_subscriptions_form_submit'),
  );
  return $form;  
}

/**
 * Subscription mass operations.
 * 
 * @param $account
 *   User account if we are administering subscriptions for this user
 */
function notifications_destination_subscriptions_operations($account = NULL) {
  $operations = array(
    'activate' => array(
      'label' => t('Activate'),
      'callback' => 'notifications_subscriptions_mass_update',
      'callback arguments' => array('updates' => array('status' => NOTIFICATIONS_SUBSCRIPTION_ACTIVE)),
    ),
    'deactivate' => array(
      'label' => t('Deactivate'),
      'callback' => 'notifications_subscriptions_mass_update',
      'callback arguments' => array('updates' => array('status' => NOTIFICATIONS_SUBSCRIPTION_INACTIVE)),
    ),
    'delete' => array(
      'label' => t('Delete'),
      'callback' => NULL,
    ),
  );
  // Block option only for administrators
  if (user_access('administer notifications') || user_access('manage all subscriptions')) {
    $operations['block'] = array(
      'label' => t('Block'),
      'callback' => 'notifications_subscriptions_mass_update',
      'callback arguments' => array('updates' => array('status' => NOTIFICATIONS_SUBSCRIPTION_BLOCKED)),
    );
  }
  $parent = t('Change send interval to');
  foreach (notifications_send_intervals($account) as $key => $name) {
    $operations['send_method-' . $key] = array(
      'label' => $name,
      'parent' => $parent,
      'callback' => 'notifications_subscriptions_mass_update',
      'callback arguments' => array('updates' => array('send_interval' => $key)),
    );
  }
  // Intervals
  return $operations;
}

/**
 * Update sending destinations when disabled a send method
 * 
 * @todo This may be better in a batch
 * 
 * @param $old
 *   Old sending method
 * @param $new
 *   Optional new sending method to replace the old one
 */
function notifications_destination_method_replace($old, $new) {
  // Purge notifications queue, we may lose some notifications but it's the safest option.
  db_query("DELETE FROM {notifications_queue} WHERE send_method = '%s'", $old);
  $deleted = db_affected_rows();
  if ($new) {
    $old_type = messaging_method_info($old, 'address_type');
    $new_type = messaging_method_info($new, 'address_type');
    // If both methods have the same address type, just replace methods
    if ($old_type == $new_type) {
      db_query("UPDATE {notifications} SET send_method = '%s' WHERE send_method = '%s'", $new, $old);
      $replaced = db_affected_rows();
    }
    else {
      // First try a bulk replacement for users that have both types of destination
      db_query("UPDATE {notifications} s INNER JOIN {messaging_destination} d ON s.uid = d.uid SET s.mdid = d.mdid, s.send_method = '%s', s.destination = d.address WHERE s.uid > 0 AND s.send_method = '%s' AND d.type = '%s'", $new, $old, $new_type);
      $replaced = db_affected_rows();
      // Now find a replacement for the rest. In case there are more than one subscription for a destination, process all at once
      // Unless this was a database field, which we should have already fixed. When no destination user has no data
      $result = db_query("SELECT DISTINCT uid, mdid FROM {notifications} WHERE uid > 0 AND send_method = '%s'", $old);
      if (!$new_type || !$field || !$table) {
        while ($subs = db_fetch_object($result)) {
          if (($account = notifications_load_user($subs->uid)) && ($dest = messaging_account_build_destination($account, $new))) {
            db_query("UPDATE {notifications} SET mdid = %d, send_method = '%s', destination = '%s' WHERE mdid = %d", $dest->mdid, $new, $dest->address, $subs->mdid);
            $replaced += db_affected_rows();
            $created++;
          }
        }
      }
    }
  }
  // Block remaining subscriptions for old sending method
  db_query("UPDATE {notifications} SET status = %d WHERE send_method = '%s'", NOTIFICATIONS_SUBSCRIPTION_BLOCKED, $old);
  $blocked = db_affected_rows();
  // Print out some messages about what's happened
  if (!empty($replaced)) {
    drupal_set_message(t("Updated @count user subscriptions with a new destination.", array('@count' => $replaced)));
  }  
  if (!empty($blocked)) {
    drupal_set_message(t("Blocked @count subscriptions as we cannot find a replacement destination.", array('@count' => $blocked)), 'warning');
  }
  if (!empty($deleted)) {
    drupal_set_message(t("Deleted @count notifications from queue, corresponding to the disabled method.", array('@count' => $deleted)));
  }
}

/**
 * Bulk create destinations for users in notifications table
 */
