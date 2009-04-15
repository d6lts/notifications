<?php
// $Id$
/**
 * @file
 *   Admin pages for User Interface for subscriptions modules
 */

/**
 *  Site-wide settings form.
 */
function notifications_ui_settings_form() {
  // Build some help notes to add to the fieldset
  $help = array(
    '<strong>' . t('Enabled') . '</strong> ' . t('If not checked no options for this subscription type will be displayed at all.'),
    '<strong>' . t('User account page') . '</strong> ' . t('If checked a new tab will be displayed under the user account Notifications page.'),
    '<strong>' . t('Create subscription page') . '</strong> ' . t('If checked a link to create this type of subscriptions will be added to the user account pages.')
  );
  $form['notifications_ui_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Visible subscription types'),
    '#options' => _notifications_subscription_types('long'),
    '#default_value' => variable_get('notifications_ui_types', array()),
    '#description' => t('Check the subscription types the UI module should show. If not checked no options for this subscription type will be displayed at all.'),
  );
  $options = array(
    'page' => t('<strong>Tab</strong>. A full tab for some subscription types will be displayed for each enabled subscription type when available.'),
    'create' => t('<strong>Create</strong>. A create link and a custom page for adding subscriptions will be available for each enabled subscription type.'),
  
  );
  // UI elements on user account pages
  $form['users'] = array(
    '#title' => t('User account pages'),
    '#type' => 'fieldset',    
    '#collapsible' => TRUE,
    '#description' => t('Check elements to display on user account pages'),
  );
  $form['users']['notifications_ui_user'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Manage own subscriptions'),
    '#default_value' => notifications_ui_user_options(),
    '#options' => $options,
    '#description' => t('Check elements to display on user account tabs for site users to manage their own subscriptions'),  
  );
  $form['users']['notifications_ui_account_options'] = array(
    '#title' => t('Subscribe to users'),
    '#type' => 'checkboxes',
    '#default_value' => variable_get('notifications_ui_account_options', array()),
    '#options' => _notifications_ui_account_options(),
    '#description' => t('Check elements to display on user account tabs for other users to subscribe to them'),     
  );

  // Several options to subscribe to content  
  $form['content'] = array(
    '#type' => 'fieldset',
    '#title' => t('Subscribe to content'),
    '#collapsible' => TRUE,
    '#description' => t('You can use the global settings here or set different options for each content type. On this second case these will be the defaults for new content types.'),  
  );
  $form['content']['notificatins_ui_per_type'] = array(
    '#type' => 'radios',
    '#default_value' => variable_get('notifications_ui_per_type', 0),
    '#options' => array(
      t('Use global settings on this page for all content types'),
      t('Set up for each content type on <a href="@content-type-settings">Administer Content Types</a>.', array('@content-type-settings' => url('admin/content/types'))),
    ),
  );  
  $form['content']['notifications_ui_node'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Global settings'),
    '#default_value' => variable_get('notifications_ui_node', array()),
    '#options' => _notifications_ui_node_options(),
    '#description' => t('Check elements to display on each node for users to subscribe / unsubscribe.'),
       
  );  
  /*
  $form['subscriptions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Visible options per subscription type'),
    '#theme' => 'notifications_ui_subscription_types',
    '#collapsible' => TRUE,
    '#description' => theme('item_list', $help),
  );
  foreach (notifications_ui_subscription_types() as $type => $info) {
    $settings = notifications_ui_type($type);
    // Each type will be in a different variable
    $key = 'notifications_ui_type_' . $type;
    $form['subscriptions'][$key]['#tree'] = TRUE;
    $form['subscriptions'][$key]['title'] = array('#value' => $info['title']);
    $form['subscriptions'][$key]['enabled'] = array('#type' => 'checkbox', '#default_value' => !empty($settings['enabled']));
    $form['subscriptions'][$key]['page'] = array('#type' => 'checkbox', '#default_value' => !empty($settings['page']), '#disabled' => empty($info['user page']));
    $form['subscriptions'][$key]['create'] = array('#type' => 'checkbox', '#default_value' => !empty($settings['create']));
  }
  */
  // Build check boxes table with content types x subscription types

  // User account options. Subscribe to author, etc...

  return system_settings_form($form);
}

/**
 * Format subscription type settings
 */
function theme_notifications_ui_subscription_types(&$elements) {
  $output = '';
  $header = array('', t('Enabled'), t('Show user account page'), t('Show create subscription page'));
  $rows = array();
  foreach (element_children($elements) as $key) {
    $rows[] = array(
      drupal_render($elements[$key]['title']),
      drupal_render($elements[$key]['enabled']),
      drupal_render($elements[$key]['page']),
      drupal_render($elements[$key]['create']),
    );
  }
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($elements);
  return $output;
}

function theme_notifications_ui_content_types(&$elements) {
  $output = '';
  $options = _notifications_ui_node_type_options();
  $header = array_merge(array(''), array_values($options));
  $rows = array();
  foreach (element_children($elements) as $key) {
    $row = array($elements[$key]['#title']);
    unset($elements[$key]['#title']);
    foreach (array_keys($options) as $index) {
      unset($elements[$key][$index]['#title']);
      $row[] = drupal_render($elements[$key][$index]);
    }
    $rows[] = $row;
  }
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($elements);
  return $output;
}