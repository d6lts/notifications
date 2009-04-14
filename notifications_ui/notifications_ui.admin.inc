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
    '<strong>' . t('User page') . '</strong> ' . t('If checked a new tab will be displayed under the user account Notifications page.'),
    '<strong>' . t('Create link') . '</strong> ' . t('If checked a link to create this type of subscriptions will be added to the user account pages.')
  );
  $form['subscriptions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Subscription types'),
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
    $form['subscriptions'][$key]['page'] = array('#type' => 'checkbox', '#default_value' => !empty($settings['page']));
    $form['subscriptions'][$key]['create'] = array('#type' => 'checkbox', '#default_value' => !empty($settings['create']));
  }

  // Use an item instead of fieldset for formatting
  $form['content'] = array('#type' => 'item', '#title' => t('Content options'));
  $form['notifications_link_teaser'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Show subscribe link with teaser'),
    '#default_value' => variable_get('notifications_link_teaser', 1),
    '#description'   => t('Uncheck to show links only in full node view. This will work only for content types that have subscription links enabled.'),
  );

  // User account options. Subscribe to author, etc...
  $form['notifications_ui_account_options'] = array(
    '#type' => 'checkboxes',
    '#title' => t('User account options'),
    '#default_value' => variable_get('notifications_ui_account_options', array()),
    '#options' => _notifications_ui_account_options(),
    '#description' => t('Check elements to display on user account tabs'),  
  );
  return system_settings_form($form);
}

/**
 * Format subscription type settings
 */
function theme_notifications_ui_subscription_types(&$elements) {
  $output = '';
  $header = array('', t('Enabled'), t('User page'), t('Create link'));
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