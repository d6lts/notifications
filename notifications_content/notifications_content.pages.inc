<?php
// $Id$
/**
 * @file
 * Subscriptions to content events
 */
 
/**
 * Admin settings form
 */
function notifications_content_settings_form() {
  // Build check boxes table with content types x subscription types
  $form['content'] = array(
    '#type' => 'fieldset',
    '#title' => t('Enabled subscription types'),
    '#weight' => -10,
    '#collapsible' => TRUE,
    '#description' => t('Check the subscription types that will be enabled. You can use the global settings here or set different options for each content type. On this second case these will be the defaults for new content types.'),
  );
  $form['content']['notifications_content_per_type'] = array(
    '#type' => 'radios',
    '#default_value' => variable_get('notifications_content_per_type', 0),
    '#options' => array(
      t('Use global settings on this page for all content types'),
      t('Set up for each content type on <a href="@content-type-settings">Administer Content Types</a>.', array('@content-type-settings' => url('admin/content/types'))),
    ),
  );  
  $form['content']['notifications_content_type'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Global options'),
    '#options' => _notifications_content_type_options(),
    '#default_value' => variable_get('notifications_content_type', array()),
    '#description' => t('Define the available subscription types that will be enabled globally'),
  );

  return system_settings_form($form);
}

/**
 * Theme content type settings
 */
function theme_notifications_content_type_settings(&$elements) {
  $output = '';
  $options = _notifications_content_type_options();
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

