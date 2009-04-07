<?php
// $Id$
/**
 * @file
 * User pages for Notifications
 */

/**
 * Menu callback. Overview page for user subscriptions.
 */
function notifications_page_user_overview($account) {
  // Build summary
  $count = array();  
  // Query all subscriptions for this user
  $result = db_query("SELECT s.type, count(s.sid) as number FROM {notifications} s WHERE s.uid = %d  GROUP BY s.type", $account->uid);
  while ($subs = db_fetch_object($result)) {
    $count[$subs->type] =  $subs->number;
  }
  $header = array(t('Type'), t('Number'));
  $rows = array();
  
  // List types and count for each type
  foreach (notifications_subscription_types() as $type => $info) {
    $access = user_access('administer notifications') || !empty($info['access']) && user_access($info['access']);
    // If no access and no count, skip this type.
    // But if no access and there are some we show the type and number without link.
    if ($access || !empty($count[$type])) {
      $rows[] = array(
         $access ? l($info['title'], "user/$account->uid/notifications/$type") : $info['title'],
         isset($count[$type]) ? $count[$type] : 0,
      );
    }
  }

  return $rows ? theme('table', $header, $rows) : t('No existing or allowed subscriptions');
}

/**
 * Menu callback add subscription
 * 
 * Presents confirmation page or not depending on confirm parameter
 */
function notifications_page_subscribe($account, $type, $fields, $values, $send_interval = NULL, $send_method = NULL) {
  global $user;

  // Check signature if present
  $params = array('subscribe', $account->uid, $type, $fields, $values);
  $signed = !empty($_GET['signature']) && $_GET['signature'] == _notifications_signature($params, !empty($_GET['confirm']));

  // Build subscriptions object
  $subscription = (object)array(
    'uid' => $account->uid,
    'type' => $type,
    'fields' => notifications_field_args($fields, $values),
    'send_interval' => $send_interval ? $send_interval : notifications_user_setting('send_interval', $account),
    'send_method' => $send_method ? $send_method : notifications_user_setting('send_method', $account),
    'event_type' => notifications_subscription_types($type, 'event_type'),
  );      
  if (notifications_user_allowed('subscription', $account, $subscription)) {
    // Display subscription information
    if (!empty($_GET['confirm']) && $signed) {
      // Subscribe, no confirmation
      notifications_save_subscription($subscription);
      drupal_set_message(t('Your subscription was activated.'));
      drupal_goto();
    }
    else {
      // Ask for confirmation
      drupal_set_title(t('Confirm your subscription'));
      return drupal_get_form('notifications_form_subscribe_confirm', $subscription, $account);
    }
  }
  else {
    drupal_set_message(t('Subscription type or parameters not allowed'), 'error');
    drupal_goto();
  }
    
  drupal_access_denied();
}

/**
 * Form for subscription confirmation
 */
function notifications_form_subscribe_confirm($form_state, $subscription, $account) {

  // Pass on simple values
  foreach (array('sid', 'uid', 'type', 'fields', 'event_type') as $field) {
    $form[$field] = array('#type' => 'value', '#value' => $subscription->$field);
  }

  // The names will be added here
  notifications_module_invoke('names', $subscription);

  $form['info'] = array(
    '#type' => 'item',
    '#title' => t('!type subscription to', array('!type' => $subscription->type_name)),
    '#value' => theme('item_list', $subscription->names),
  );

  // Additional parameters
  $form['send_interval'] = array(
    '#type' => 'select',
    '#title' => t('Send interval'),
    '#options' => _notifications_send_intervals(),
    '#default_value' => $subscription->send_interval,
  );
  $send_methods = _notifications_send_methods($account);
  $form['send_method'] = array(
    '#type' => 'select',
    '#title' => t('Send method'),
    '#options' => $send_methods,
    '#default_value' => $subscription->send_method,
    '#disabled' => count($send_methods) < 2,
  );

  $form['confirm'] = array('#type' => 'submit', '#value' => t('Subscribe'));
  $form['cancel'] = array('#type' => 'submit', '#value' => t('Cancel'));
  return $form;
}

/**
 * Process form submission
 */
function notifications_form_subscribe_confirm_submit($form, &$form_state) {
  $subscription = (object)$form_state['values'];
  switch ($form_state['values']['op']) {
    case t('Subscribe'):
      notifications_save_subscription($subscription);
      drupal_set_message(t('Your subscription was activated.'));
      break;
    case t('Cancel'):
      drupal_set_message(t('Your subscription was cancelled'));
      break;
  }
  return 'user/'. $subscription->uid .'/notifications';   
}

/**
 * Process arguments and return an array of field/value pairs
 */
function notifications_field_args($fields, $values) {
  $names = explode(',', $fields);
  $params = explode(',', $values);
  return array_combine($names, $params);  
}

/**
 * Menu callback for unsubscribe page
 * 
 * @param $type
 *   Either 'sid' or 'uid' (for unsubscribe all)
 * @param $id
 *   Subscription id or user id, depending on type
 */
function notifications_page_unsubscribe($type, $id) {
  global $user;
  
  // Check signature if present
  $signed = !empty($_GET['signature']) && $_GET['signature'] == _notifications_signature(array('unsubscribe', $type, $id), !empty($_GET['confirm']));
  
  // Determine subscription and user depending on type
  if ($type == 'sid' && is_numeric($id) && ($subscription = notifications_load_subscription($id))) {
    $account = NULL;
    $uid = $subscription->uid;
  }
  elseif ($type == 'uid' && is_numeric($id) && ($account = user_load($id)) && $account->uid) {
    $subscription = NULL;
    $uid = $account->uid;
  }
  
  // Check permissions and present confirmation form or not depending on parameters
  if (($account || $subscription) && $uid && 
      (user_access('administer notifications') || ($user->uid == $uid) || $signed)) {
    // Skip confirmation page when requested and the signature is ok
    if (!empty($_GET['confirm']) && $signed) {
      if ($subscription) {
        notifications_delete_subscription($subscription->sid);
        drupal_set_message(t('Your subscription has been removed.'));
      }
      elseif ($account) {
        notifications_delete_subscriptions(array('uid' => $account->uid));
        drupal_set_message(t('All your subscriptions have been removed.'));
      }
      drupal_goto();
    } 
    elseif ($account && !db_result(db_query("SELECT COUNT(*) FROM {notifications} WHERE uid = %d", $account->uid))) {
      return t("You don't have any subscription on this site.");
    }
    else {
      // Display confirmation form
      return drupal_get_form('notifications_form_unsubscribe_confirm', $subscription, $account);
    }
  }
  drupal_access_denied();
}


/**
 * Form for unsubscription confirmation
 * 
 * It works for both single subscription or account (all subscriptions)
 */
function notifications_form_unsubscribe_confirm($form_state, $subscription, $account = NULL) {

  // Pass on subscription values
  $form['subscription'] = array('#type' => 'value', '#value' => $subscription);
  $form['account'] = array('#type' => 'value', '#value' => $account);
  
  if ($subscription) {
    $message = t('Are you sure you want to delete this subscription?');
    // The names will be added here
    notifications_module_invoke('names', $subscription);
  
    $form['info'] = array(
      '#type' => 'item',
      '#title' => t('!type subscription to', array('!type' => $subscription->type_name)),
      '#value' => theme('item_list', $subscription->names),
    );    
  }
  else {
    $message = t('Are you sure you want to remove all your subscriptions on this site?');
  }

  return confirm_form($form,
    $message,
    isset($_GET['destination']) ? $_GET['destination'] : '',
    t('This action cannot be undone.'),
    t('Unsubscribe'),
    t('Cancel')
  );
}

/**
 * Process form submission
 */
function notifications_form_unsubscribe_confirm_submit($form, &$form_state) {
  $subscription = $form_state['values']['subscription'];
  $account = $form_state['values']['account'];
  switch ($form_state['values']['op']) {
    case t('Unsubscribe'):
      if ($subscription) {
        notifications_delete_subscription($subscription->sid);
        drupal_set_message(t('Your subscription has been removed.'));
      }
      elseif ($account) {
        notifications_delete_subscriptions(array('uid' => $account->uid));
        drupal_set_message(t('All your subscriptions have been removed.'));
      }
      break;
    case t('Cancel'):
      // Do nothing, not worth showing a message, just get back
      break;
  } 
}

/**
 * Edit subscription
 */
function notifications_subscription_form($form_state, $subscription) {
  // The names will be added here
  notifications_module_invoke('names', $subscription);
  $form['info'] = array(
    '#type' => 'item',
    '#title' => t('!type subscription to', array('!type' => $subscription->type_name)),
    '#value' => theme('item_list', $subscription->names),
  );

  // Subscription values
  
  $form['subscription'] = array('#type' => 'value', '#value' => $subscription);

  $form['send_interval'] = array(
    '#type' => 'select',
    '#title' => t('Send interval'),
    '#options' => _notifications_send_intervals(),
    '#default_value' => $subscription->send_interval,
  );
  $send_methods = _notifications_send_methods();
  $form['send_method'] = array(
    '#type' => 'select',
    '#title' => t('Send method'),
    '#options' => $send_methods,
    '#default_value' => $subscription->send_method,
  );
  $form['status'] = array(
    '#type' => 'radios',
    '#title' => t('Status'),
    '#options' => _notifications_subscription_status(),
    '#default_value' => $subscription->status,
  );

  $form['save'] = array('#type' => 'submit', '#value' => t('Save'));
  
  $form['delete'] = array('#type' => 'submit', '#value' => t('Delete'));
  
  return $form;  
}

/**
 * Save edited subscription
 */
function notifications_subscription_form_submit($form, $form_state) {
  // Rebuild subscription object
  $subscription = $form_state['values']['subscription'];
  foreach (array('send_interval', 'send_method', 'status') as $field) {
    if (isset($form_state['values'][$field])) {
      $subscription->$field = $form_state['values'][$field];
    }
  }
  switch($form_state['values']['op']) {
    case t('Save'):
      notifications_save_subscription($subscription);
      break;
    case t('Delete'):
      notifications_delete_subscription($subscription->sid);
      break;
  }
}