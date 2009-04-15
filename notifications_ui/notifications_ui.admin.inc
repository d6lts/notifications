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
  
  // Build check boxes table with content types x subscription types
  $form['content'] = array(
    '#type' => 'fieldset',
    '#title' => t('Visible options per content type'),
    '#collapsible' => TRUE,
    '#theme' => 'notifications_ui_content_types',
    '#description' => t('Define the subscription options that will be visible for each content type. These can be configured too on <a href="@content-type-settings">Administer Content Types</a>.', array('@content-type-settings' => url('admin/content/types'))),
  );
  foreach (node_get_types('names') as $content_type => $type_name) {
    $key = 'notifications_node_ui_' . $content_type;
    $settings = variable_get($key, array());
    // These will be expanded to individual checkboxes before theming
    $form['content'][$key] = array(
      '#title' => check_plain($type_name),
      '#type' => 'checkboxes',
      '#default_value' => notifications_ui_node_options($content_type),
      '#options' => _notifications_ui_node_type_options(),
    );
  }
  // User account options. Subscribe to author, etc...
  $form['notifications_ui_account_options'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Display on user account page'),
    '#default_value' => variable_get('notifications_ui_account_options', array()),
    '#options' => _notifications_ui_account_options(),
    '#description' => t('Check elements to display on user account tabs for other users to subscribe to them'),  
  );
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