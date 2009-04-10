<?php
// $Id$
/**
 * @file
 *   User pages for User Interface for subscriptions modules
 */
 
/**
 * User add subscription
 */
function notifications_ui_page_user_add($account, $type = NULL) {
  drupal_set_title(t('Add subscription'));
  if ($type && notifications_ui_access_user_add($account, $type)) {    
    module_load_include('inc', 'notifications', 'notifications.pages');
    return drupal_get_form('notifications_add_subscription_form', $account, $type);
  }
  else {    
    foreach (notifications_ui_subscription_types() as $key => $type) {
      if (notifications_ui_type($key, 'create' ) && notifications_access_user_add($account, $key)) {
        $options[] = array(
          'title' => $type['title'],
          'href'  => "user/$account->uid/notifications/add/$key",
          'description' => !empty($type['description']) ? $type['description'] : '',
        );
      }
    }
    return theme('notifications_ui_add_list', $options);
  }  
}

/**
 * Display the list of available subscription types for creation
 *
 * @ingroup themeable
 */
function theme_notifications_ui_add_list($content) {
  $output = '';

  if ($content) {
    $output = '<dl class="subscriptions-type-list">';
    foreach ($content as $item) {
      $output .= '<dt>'. l($item['title'], $item['href']) .'</dt>';      
      $output .= '<dd>'. filter_xss_admin($item['description']) .'</dd>';
    }
    $output .= '</dl>';
  }
  return $output;
}