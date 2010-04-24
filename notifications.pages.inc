<?php
// $Id$
/**
 * @file
 * User pages for Notifications
 */

/**
 * Menu callback. Overview page for user subscriptions.
 * 
 * We create the page as a form so it can be altered
 */
function notifications_user_overview($form_state, $account) {
  $form['account'] = array('#type' => 'value', '#value' => $account);
  
  $path = 'user/' . $account->uid;
  
  // Status, send method, interval
  $count = db_query("SELECT status, COUNT(*) AS num FROM {notifications} WHERE uid = %d GROUP BY status", $account->uid);
  $status_list = _notifications_subscription_status();
  while ($current = db_fetch_object($count)) {
    $status[$current->status] = t('You have %count @status subscriptions.', array('%count' => $current->num, '@status' => $status_list[$current->status]));
  }
  if (empty($status)) {
    $status['none'] = t('You don\'t have any subscriptions yet.');
  }
  $send_intervals = notifications_send_intervals();
  $send_methods = _notifications_send_methods($account);
  if ($method = notifications_user_setting('send_method', $account)) {
    if (isset($send_methods[$method])) {      
      $status['method'] = t('Your default sending method for new subscriptions is @send_method', array('@send_method' => $send_methods[$method]));
    }
  }
  $interval = notifications_user_setting('send_interval', $account);
  if (isset($send_intervals[$interval])) {      
    $status['interval'] = t('Your default sending interval for new subscriptions is @send_interval', array('@send_interval' => $send_intervals[$interval]));
  }
  
  $form['status'] = array('#type' => 'item', '#weight' => 10, '#title' => t('Current status'), '#value' => theme('item_list', $status));
  
  // Build shortcut tips
  if (notifications_access_user($account, 'manage')) {
    $tips['admin'] = l(t('Administer your subscriptions'), "$path/notifications/subscriptions");
  }
  $tips['edit'] = l(t('Edit your notifications settings'), "$path/edit");
  
  // Enable / disable all subscriptions
  if (notifications_access_user($account, 'maintain')) {
    if (!empty($status[NOTIFICATIONS_SUBSCRIPTION_ACTIVE])) {
      $tips['disable'] = l(t('Temporarily disable all your subscriptions'), "$path/notifications/update/disable");
    }
    if (!empty($status[NOTIFICATIONS_SUBSCRIPTION_INACTIVE]) || !empty($status[NOTIFICATIONS_SUBSCRIPTION_BLOCKED])) {
      $tips['enable'] = l(t('Enable all your subscriptions'), "$path/notifications/update/enable");
    }    
  }
  $link = notifications_get_link('unsubscribe', array('uid' => $account->uid, 'destination' => TRUE));
  $tips['cancel'] = l(t('Cancel all your subscriptions'), $link['href'], $link['options']);
  
  //$output .= theme('item_list', $tips, t('You can'));
  $form['tips'] = array('#type' => 'item', '#weight' => 20, '#title' => t('You can'), '#value' => theme('item_list', $tips));
   
  return $form;  
}

/**
 * Manage destination form. Edit subscriptions for a destination
 */
function notifications_edit_destination_form($form_state, $destination) {
  notifications_include('destination.inc');
  $account = !empty($destination->uid) ? messaging_load_user($destination->uid) : drupal_anonymous_user();
  $form = notifications_destination_view_subform($destination);
  
  return $form;
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
  $subscription = notifications_build_subscription(array(
    'uid' => $account->uid,
    'type' => $type,
    'send_interval' => $send_interval,
    'send_method' => $send_method,
  ));
  $subscription->add_field_args($fields, $values);
  $subscription->set_account($account);
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
      return drupal_get_form('notifications_subscription_confirm_form', $subscription);
    }
  }
  else {
    drupal_set_message(t('Subscription type or parameters not allowed'), 'error');
    drupal_goto();
  }
    
  drupal_access_denied();
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
        notifications_subscription_delete($subscription->sid);
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
    // The subscription description will be added here
    $form['info'] = $subscription->form_info();    
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
 * Process unsubscribe form submission
 */
function notifications_form_unsubscribe_confirm_submit($form, &$form_state) {
  $subscription = $form_state['values']['subscription'];
  $account = $form_state['values']['account'];
  switch ($form_state['values']['op']) {
    case t('Unsubscribe'):
      if ($subscription) {
        notifications_subscription_delete($subscription->sid);
        drupal_set_message(t('Your subscription has been removed.'));
      }
      elseif ($account) {
        notifications_delete_subscriptions(array('uid' => $account->uid));
        drupal_set_message(t('All your subscriptions have been removed.'));
      }
      $form_state['redirect'] = '';
      break;
    case t('Cancel'):
      // Do nothing, not worth showing a message, just get back
      $form_state['redirect'] = '';
      break;
  }
}

/**
 * Edit subscription page that can be accessed with signed link
 */
function notifications_subscription_edit_page($subscription) {
  global $user;

  // Aditional checks for anonymous subscriptions so not everybody can access them
  if (!$subscription->uid && !user_access('administer notifications') && !user_access('manage subscriptions') && !notifications_check_signature()) {
    drupal_access_denied(); 
  }
  else {
    return drupal_get_form('notifications_subscription_edit_form', $subscription);
  }
}

/**
 * Base subscription form. Present the main fields and check all values
 */
function notifications_subscription_base_form($form_state, $subscription, $destination_path = NULL) {
  // We may have a destination query string instead of a destination path
  if (!empty($_REQUEST['destination'])) {
    $destination_path = $_REQUEST['destination'];
  }
  if (isset($destination_path)) {
    $form['#redirect'] = $destination_path;
  }
  
  // Check the subscription object is built properly
  $subscription = notifications_build_subscription($subscription);
  // Pass on whole subscription and then the fields to be saved along
  $form['subscription'] = array('#type' => 'value', '#value' => $subscription);
  $account = $subscription->get_user();
  $save_fields = array();

  // The subscription description will be added here, fields may be editable or not
  if (!$subscription->is_editable()) {
    // Just display general information, no editable fields
    $form['info'] = $subscription->form_info();
  }
  else {
    $form['info'] = $subscription->fields_subform();
    $save_fields[] = 'fields'; 
  }
  if ($subscription->is_instance()) {
    $form['info']['#title'] = t('Edit subscription');
  }
  else {
    $form['info']['#title'] = t('Create @type subscription', array('@type' => $subscription->get_title()));
  }

  // Send interval, visible only if more than one
  if ($send_intervals = notifications_send_intervals($account)) {
    $save_fields[] = 'send_interval';
    if (count($send_intervals) == 1) {
      $form['send_interval'] = array('#type' => 'value', '#value' => key($send_intervals));
    }
    else {
      $form['notifications']['send_interval'] = array(
        '#type' => 'select',
        '#title' => t('Send interval'),
        '#options' => $send_intervals,
        '#default_value' => $subscription->send_interval,
      );
    }
  }
  else {
    drupal_set_message(t('No sending intervals available.'), 'error');
    $form['#error']['send_interval'] = TRUE;
  }
  // Send method, visible only if more than one
  if ($send_methods = notifications_send_methods($account)) {
    $save_fields[] = 'send_method';
    if (count($send_methods) == 1) {
      $form['send_method'] = array('#type' => 'value', '#value' => key($send_methods));
    }
    else {
      $form['notifications']['send_method'] = array(
        '#type' => 'select',
        '#title' => t('Send method'),
        '#options' => $send_methods,
        '#default_value' => $subscription->send_method,
        '#disabled' => count($send_methods) < 2,
      );
    }
  }
  else {
    drupal_set_message(t('No sending methods available.'), 'error');
    $form['#error']['send_method'] = TRUE;
  }
  // Add notifications fieldset if send method or send_interval
  if (!empty($form['notifications'])) {
    $form['notifications']['#title'] = t('Notifications');
    $form['notifications']['#type'] = 'fieldset';
    $form['notifications']['#description'] = t('How do you want to get notifications for this subscription.');
  }
  // Status field, show blocked only to administrators
  if ($subscription->is_instance()) {
    $save_fields[] = 'status';
    $status = _notifications_subscription_status();
    if (!user_access('administer notifications') && !user_access('manage all subscriptions')) {
      unset($status[NOTIFICATIONS_SUBSCRIPTION_BLOCKED]);
    }
    $form['status'] = array(
      '#type' => 'radios',
      '#title' => t('Status'),
      '#options' => $status,
      '#default_value' => $subscription->status,
      '#description' => t('You can temporarily disable this subscription for not getting notifications.')
    );
  }
  // Fields to be saved on submit. If error we just show the 'Cancel' button.
  $form['subscription_fields'] = array('#type' => 'value', '#value' => $save_fields);
  
  // Add form buttons depending on what we are doing
  if ($subscription->is_instance()) {
    $form['actions']['save'] = array('#type' => 'submit', '#value' => t('Save'));
    $form['actions']['delete'] = array('#type' => 'submit', '#value' => t('Delete'));
  }
  elseif ($subscription->is_editable()) {
    $form['actions']['subscribe'] = array('#type' => 'submit', '#value' => t('Create subscription'));
  }
  else {
    // Subscription parameters are fixed so this is just a confirmation form
    $form['actions']['subscribe'] = array('#type' => 'submit', '#value' => t('Subscribe'));
  }
  if (isset($destination_path)) {
    $form['actions']['cancel'] = array('#value' => l(t('Cancel'), $destination_path));
  }
  // Add the right callbacks
  $form['#submit'][] = 'notifications_subscription_form_submit';
  $form['#validate'][] = 'notifications_subscription_form_validate';
  return $form;  
}

/**
 * Validate subscription
 */
function notifications_subscription_base_form_validate($form, &$form_state) {
  Notifications_Subscription::validate_submission($form_state);
}

/**
 * Save edited subscription
 */
function notifications_subscription_base_form_submit($form, &$form_state) {
  $subscription = Notifications_Subscription::build_submission($form_state);

  switch ($form_state['values']['op']) {
    case t('Save'):
      // Subscription has been edited
      $result = notifications_save_subscription($subscription);
      if ($result) {
        drupal_set_message(t('Your subscription has been updated.'));
      }
      break;
    case t('Subscribe'):
    case t('Create subscription'):
      // We are creating a new subscription
      $result = notifications_save_subscription($subscription);
      if ($result === SAVED_NEW) {
        drupal_set_message(t('Your subscription has been created.'));
      }
      elseif ($result === SAVED_UPDATED) {
        drupal_set_message(t('Your subscription has been created updating an existing one of the same type.'));
      } else {
        drupal_set_message(t('Your subscription cannot be created.'), 'error');
      }
      break;
    case t('Delete'):
      notifications_subscription_delete($subscription->sid);
      $form_state['redirect'] = 'user/'. $subscription->uid .'/notifications';
      break;
  }
}

/**
 * Bulk update all user subscriptions, confirmation form
 * 
 * @param $account
 *   User account
 * @param $operation
 *   Operation to perform: enable, disable
 */
function notifications_update_user_subscriptions($form_state, $account, $operation) {
  $form['account'] = array('#type' => 'value', '#value' => $account);
  $form['operation'] = array('#type' => 'value', '#value' => $operation);  
  $destination = 'user/' . $account->uid . '/notifications';
  $form['#redirect'] = $destination;
  if ($operation == 'enable') {
    return confirm_form($form,
                      t('Are you sure you want to enable all your subscriptions?'),
                      $destination, t('You will get notifications again for all of them.'),
                      t('Enable'), t('Cancel'));
  }
  elseif ($operation == 'disable') {
    return confirm_form($form,
                      t('Are you sure you want to disable all your subscriptions?'),
                      $destination, t('You will stop getting notifications until you enable them again.'),
                      t('Disable'), t('Cancel'));    
  }
  else {
    drupal_access_denied();
  }
}

/**
 * Mass update user subscriptions
 */
function notifications_update_user_subscriptions_submit($form, $form_state) {
  $account = $form_state['values']['account'];
  
  switch ($form_state['values']['operation']) {
    case 'enable':
      // Enable also blocked subscriptions just in case there are some
      db_query('UPDATE {notifications} SET status = %d WHERE uid = %d', NOTIFICATIONS_SUBSCRIPTION_ACTIVE, $account->uid);
      drupal_set_message(t('All your subscriptions have been enabled.'));
      break;
    case 'disable':
      db_query('UPDATE {notifications} SET status = %d WHERE status = %d AND uid = %d', NOTIFICATIONS_SUBSCRIPTION_INACTIVE, NOTIFICATIONS_SUBSCRIPTION_ACTIVE, $account->uid);
      notifications_queue_clean(array('uid' => $account->uid));
      drupal_set_message(t('All your subscriptions have been disabled.'));
      break;
  }
}

/**
 * Theme function for fields in form
 */
function theme_notifications_subscription_fields($elements) {
  $header = array(t('Field type'), t('Value'));
  $rows = array();
  foreach (element_children($elements['name']) as $key) {
    $rows[] = array(
      drupal_render($elements['name'][$key]),
      drupal_render($elements['value'][$key]),
    );
  }
  $output = theme('table', $header, $rows);
  $output .= drupal_render($elements);
  return $output;
}